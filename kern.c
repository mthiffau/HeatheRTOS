#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "kern.h"

#include "xbool.h"
#include "xarg.h"
#include "xassert.h"

#include "ctx_switch.h"
#include "intr.h"
#include "syscall.h"
#include "link.h"

#include "u_syscall.h"
#include "u_init.h"

#include "cpumode.h"
#include "ts7200.h"
#include "bwio.h"

#define EXC_VEC_INSTR       0xe59ff018 /* ldr pc, [pc, #+0x18] */
#define EXC_VEC_SWI         ((unsigned int*)0x8)
#define EXC_VEC_FP(i)       (*((void**)((void*)(i) + 0x20)))

static void task_enqueue(struct task_desc *td, struct task_desc **q);
static struct task_desc *task_dequeue(struct task_desc **q);

int
main(void)
{
    struct kern kern;

    /* Set up kernel state and create initial user task */
    kern_init(&kern);

    /* Run init */
    task_create(&kern, -1, U_INIT_PRIORITY, &u_init_main);

    /* Main loop */
    for (;;) {
        struct task_desc *active;
        uint32_t          intr;

        active = task_schedule(&kern);
        if (active == NULL) {
            if (kern.ntasks == 0)
                break;
            else
                continue; /* FIXME this won't do once IRQs are in use */
        }

        intr = ctx_switch(active);
        kern_handle_intr(&kern, active, intr);
    }

    return 0;
}

void
kern_init(struct kern *kern)
{
    uint32_t i, mem_avail;

    /* Turn off UART FIFOs. */
    bwsetfifo(COM1, OFF);
    bwsetfifo(COM2, OFF);

    /* Install SWI handler. */
    *EXC_VEC_SWI = EXC_VEC_INSTR;
    EXC_VEC_FP(EXC_VEC_SWI) = &kern_entry_swi;

    /* 1MB reserved for kernel stack, move down to next 4k boundary */
    kern->stack_mem_top = (void*)(((uint32_t)&i - 0x100000) & 0xfffff000);

    /* Find largest number of 4k pages it is safe to allocate per task */
    mem_avail = kern->stack_mem_top - ProgramEnd;
    kern->stack_size = mem_avail / 0x1000 / MAX_TASKS * 0x1000;

    /* All tasks are free to begin with */
    kern->ntasks = 0;
    kern->free_tasks = &kern->tasks[0];
    for (i = 0; i < MAX_TASKS; i++) {
        struct task_desc *td = &kern->tasks[i];
        td->tid  = -1;
        td->next = &kern->tasks[i + 1];
    }

    kern->tasks[MAX_TASKS - 1].next = NULL;

    /* All ready queues are empty */
    for (i = 0; i < N_PRIORITIES; i++)
        kern->rdy_queues[i] = NULL;
}

void
kern_handle_intr(struct kern *kern, struct task_desc *active, uint32_t intr)
{
    switch (intr) {
    case INTR_SWI:
        kern_handle_swi(kern, active);
        break;
    default:
        panic("received unknown interrupt 0x%x\n", intr);
    }
}

void
kern_handle_swi(struct kern *kern, struct task_desc *active)
{
    uint32_t syscall = *((uint32_t*)active->regs->pc - 1) & 0x00ffffff;
    switch (syscall) {
    case SYSCALL_CREATE:
        active->regs->r0 = (uint32_t)task_create(
            kern,
            active->tid,
            (int)active->regs->r0,
            (void(*)(void))active->regs->r1);
        task_ready(kern, active);
        break;
    case SYSCALL_MYTID:
        active->regs->r0 = (uint32_t)active->tid;
        task_ready(kern, active);
        break;
    case SYSCALL_MYPARENTTID:
        active->regs->r0 = (uint32_t)active->parent_tid;
        task_ready(kern, active);
        break;
    case SYSCALL_PASS:
        task_ready(kern, active);
        break;
    case SYSCALL_EXIT:
        active->state = TASK_STATE_ZOMBIE; /* leave off ready queue */
        kern->ntasks--;
        break;
    default:
        panic("received unknown syscall 0x%x\n", syscall);
    }
}

tid_t
task_create(
    struct kern *kern,
    tid_t parent_tid,
    int priority,
    void (*task_entry)(void))
{
    struct task_desc *td;
    uint8_t ix;
    void *stack;

    if (priority < 0 || priority >= N_PRIORITIES)
        return -1; /* invalid priority */

    td = kern->free_tasks;
    if (td == NULL)
        return -2; /* no more task descriptors */

    assert(td->tid < 0);
    kern->free_tasks = td->next;

    /* Guaranteed to succeed from this point */
    kern->ntasks++;

    /* Initialize task */
    ix             = td - kern->tasks;
    stack          = kern->stack_mem_top - ix * kern->stack_size;
    td->regs       = (struct task_regs*)(stack) - 1; /* leave room */
    td->regs->spsr = cpumode_bits(MODE_USR);         /* interrupts enabled */
    td->regs->lr   = (uint32_t)&Exit; /* call Exit on return of task_entry */
    td->regs->pc   = (uint32_t)task_entry;

    td->tid        = ix; /* FIXME */
    td->parent_tid = parent_tid;
    td->priority   = priority;

    task_ready(kern, td);
    return td->tid;
}

void
task_ready(struct kern *kern, struct task_desc *td)
{
    td->state = TASK_STATE_READY;
    task_enqueue(td, &kern->rdy_queues[td->priority]);
}

/* NB. lower numbers are higher priority! */
struct task_desc*
task_schedule(struct kern *kern)
{
    int i;
    for (i = 0; i < N_PRIORITIES; i++) {
        struct task_desc *next = task_dequeue(&kern->rdy_queues[i]);
        if (next != NULL) {
            next->state = TASK_STATE_ACTIVE;
            return next;
        }
    }
    return NULL;
}

static void
task_enqueue(struct task_desc *td, struct task_desc **q)
{
    if (*q == NULL) {
        *q = td;
        td->next = td;
        td->prev = td;
    } else {
        struct task_desc *first, *last;
        first = *q;
        last  = first->prev;
        td->next = first;
        td->prev = last;
        last->next  = td;
        first->prev = td;
    }
}

static struct task_desc*
task_dequeue(struct task_desc **q)
{
    struct task_desc *result, *newfirst, *last;
    result = *q;
    if (result == NULL)
        return NULL;

    newfirst = result->next;
    last     = result->prev;
    if (newfirst == result) {
        *q = NULL;
    } else {
        *q = newfirst;
        newfirst->prev = last;
        last->next     = newfirst;
    }

    return result;
}
