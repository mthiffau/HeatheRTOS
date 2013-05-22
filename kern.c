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

static void task_enqueue(struct kern*, struct task_desc*, struct task_queue*);
static struct task_desc *task_dequeue(struct kern*, struct task_queue*);

static inline int
ctz16(uint16_t x)
{
    int tz = 1;
    if ((x & 0xff) == 0) {
        x >>= 8;
        tz += 8;
    }
    if ((x & 0xf) == 0) {
        x >>= 4;
        tz += 4;
    }
    if ((x & 0x3) == 0) {
        x >>= 2;
        tz += 2;
    }
    tz -= x & 0x1;
    return tz;
}

int
main(void)
{
    struct kern kern;

    /* Set up kernel state and create initial user task */
    kern_init(&kern);

    /* Run init - it's its own parent! */
    task_create(&kern, 0, U_INIT_PRIORITY, &u_init_main);

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
    kern->free_tasks.head_ix = 0;
    kern->free_tasks.tail_ix = MAX_TASKS - 1;
    for (i = 0; i < MAX_TASKS; i++) {
        struct task_desc *td = &kern->tasks[i];
        td->state_prio = TASK_STATE_FREE; /* prio is arbitrary on init */
        td->tid_seq    = 0;
        td->next_ix    = (i + 1 == MAX_TASKS) ? TASK_IX_NULL : (i + 1);
    }

    /* All ready queues are empty */
    kern->rdy_queue_ne = 0;
    for (i = 0; i < N_PRIORITIES; i++) {
        struct task_queue *q = &kern->rdy_queues[i];
        q->head_ix = TASK_IX_NULL;
        q->tail_ix = TASK_IX_NULL;
    }
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
            TASK_PTR2IX(kern, active),
            (int)active->regs->r0,
            (void(*)(void))active->regs->r1);
        task_ready(kern, active);
        break;
    case SYSCALL_MYTID:
        active->regs->r0 = (uint32_t)TASK_TID(kern, active);
        task_ready(kern, active);
        break;
    case SYSCALL_MYPARENTTID:
        active->regs->r0 = (uint32_t)TASK_TID(
            kern,
            TASK_IX2PTR(kern, active->parent_ix));
        task_ready(kern, active);
        break;
    case SYSCALL_PASS:
        task_ready(kern, active);
        break;
    case SYSCALL_EXIT:
        TASK_SET_STATE(active, TASK_STATE_FREE);
        active->tid_seq++;
        task_enqueue(kern, active, &kern->free_tasks);
        kern->ntasks--;
        break;
    default:
        panic("received unknown syscall 0x%x\n", syscall);
    }
}

tid_t
task_create(
    struct kern *kern,
    uint8_t parent_ix,
    int priority,
    void (*task_entry)(void))
{
    struct task_desc *td;
    uint8_t ix;
    void *stack;

    if (priority < 0 || priority >= N_PRIORITIES)
        return -1; /* invalid priority */

    td = task_dequeue(kern, &kern->free_tasks);
    if (td == NULL)
        return -2; /* no more task descriptors */

    assert((td->state_prio & TASK_STATE_MASK) == TASK_STATE_FREE);

    /* Guaranteed to succeed from this point */
    kern->ntasks++;

    /* Initialize task. */
    ix             = TASK_PTR2IX(kern, td);
    td->parent_ix  = parent_ix;
    TASK_SET_PRIO(td, priority); /* task_ready() will set state */

    stack          = kern->stack_mem_top - ix * kern->stack_size;
    td->regs       = (struct task_regs*)stack - 1; /* leave room for regs */
    td->regs->spsr = cpumode_bits(MODE_USR);       /* interrupts enabled */
    td->regs->sp   = (uint32_t)stack;
    td->regs->lr   = (uint32_t)&Exit; /* call Exit on return of task_entry */
    td->regs->pc   = (uint32_t)task_entry;

    task_ready(kern, td);
    return TASK_TID(kern, td);
}

void
task_ready(struct kern *kern, struct task_desc *td)
{
    int prio = TASK_PRIO(td);
    TASK_SET_STATE(td, TASK_STATE_READY);
    task_enqueue(kern, td, &kern->rdy_queues[prio]);
    kern->rdy_queue_ne |= 1 << prio;
}

/* NB. lower numbers are higher priority! */
struct task_desc*
task_schedule(struct kern *kern)
{
    int prio;
    struct task_queue *q;
    struct task_desc  *td;

    if (kern->rdy_queue_ne == 0)
        return NULL;

    prio = ctz16(kern->rdy_queue_ne);
    q    = &kern->rdy_queues[prio];
    td   = task_dequeue(kern, q);
    assert(td != NULL); /* if not, rdy_queue_ne was inconsistent */

    if (q->head_ix == TASK_IX_NULL)
        kern->rdy_queue_ne &= ~(1 << prio);

    TASK_SET_STATE(td, TASK_STATE_ACTIVE);
    return td;
}

static void
task_enqueue(struct kern *kern, struct task_desc *td, struct task_queue *q)
{
    uint8_t ix = TASK_PTR2IX(kern, td);
    td->next_ix = TASK_IX_NULL;
    if (q->head_ix == TASK_IX_NULL) {
        q->head_ix = ix;
        q->tail_ix = ix;
    } else {
        TASK_IX2PTR(kern, q->tail_ix)->next_ix = ix;
        q->tail_ix = ix;
    }
}

static struct task_desc*
task_dequeue(struct kern *kern, struct task_queue *q)
{
    struct task_desc *td;
    if (q->head_ix == TASK_IX_NULL)
        return NULL;

    td = TASK_IX2PTR(kern, q->head_ix);
    q->head_ix = td->next_ix;
    return td;
}
