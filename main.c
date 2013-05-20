#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "xarg.h"
#include "static_assert.h"

#include "cpumode.h"
#include "task.h"
#include "intr.h"
#include "ctx_switch.h"
#include "u_init.h"

#include "link.h"
#include "ts7200.h"
#include "bwio.h"

#define EXC_VEC_INSTR       0xe59ff018 /* ldr pc, [pc, #+0x18] */
#define EXC_VEC_SWI         ((unsigned int*)0x8)
#define EXC_VEC_FP(i)       (*((void**)((void*)(i) + 0x20)))

#define MAX_TASKS           128  /* must be <= 255 */
#define N_PRIORITIES        16

extern uint32_t _BssStart;

struct kern_state {
    void             *stack_mem_top;
    size_t            stack_size;
    struct task_desc  tasks[MAX_TASKS];
    struct task_desc *rdy_queues[N_PRIORITIES];
    struct task_desc *free_tasks;
};

void
__attribute__((noreturn))
panic(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bwformat(COM2, fmt, args);
    va_end(args);

    /* hang */
    for (;;) { }
}

void
_assert(bool x, const char *msg)
{
    if (!x)
        panic("%s\n", msg);
}

#define assert(x) _assert((x), "assertion (" #x ") failed!")

/* Initialize kernel state. Tasks stack data must be stored
 * in locations >= low, and < high. */
void
kst_init(struct kern_state *kst)
{
    uint32_t i, mem_avail;
    /* 1MB reserved for kernel stack, move down to next 4k boundary */
    kst->stack_mem_top = (void*)(((uint32_t)&i - 0x100000) & 0xfffff000);

    /* Find largest number of 4k pages it is safe to allocate per task */
    mem_avail = kst->stack_mem_top - ProgramEnd;
    kst->stack_size = mem_avail / 0x1000 / MAX_TASKS * 0x1000;

    /* All tasks are free to begin with */
    kst->free_tasks = &kst->tasks[0];
    for (i = 0; i < MAX_TASKS; i++) {
        struct task_desc *td = &kst->tasks[i];
        td->tid  = -1;
        td->next = &kst->tasks[i + 1];
    }

    kst->tasks[MAX_TASKS - 1].next = NULL;

    /* All ready queues are empty */
    for (i = 0; i < N_PRIORITIES; i++)
        kst->rdy_queues[i] = NULL;
}

void
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

struct task_desc*
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

/* Create a new task */
tid_t
task_create(
    struct kern_state *kst,
    tid_t parent_tid,
    int priority,
    void (*task_entry)(void))
{
    struct task_desc *td;
    uint8_t ix;
    void *stack;

    if (priority < 0 || priority >= N_PRIORITIES)
        return -1; /* invalid priority */

    td = kst->free_tasks;
    if (td == NULL)
        return -2; /* no more task descriptors */

    assert(td->tid < 0);
    kst->free_tasks = td->next;

    /* Initialize task */
    ix       = td - kst->tasks;
    stack    = kst->stack_mem_top - ix * kst->stack_size;
    td->regs = (struct task_regs*)(stack) - 1; /* room for regs structure */
    td->regs->spsr = cpumode_bits(MODE_USR);   /* all interrupts enabled */
    td->regs->pc   = (uint32_t)task_entry;

    td->tid        = ix; /* FIXME */
    td->parent_tid = parent_tid;
    td->priority   = priority;
    td->state      = TASK_STATE_READY;
    task_enqueue(td, &kst->rdy_queues[priority]);

    return td->tid;
}

/* NB. lower numbers are higher priority! */
struct task_desc*
task_schedule(struct kern_state *kst)
{
    int i;
    for (i = 0; i < N_PRIORITIES; i++) {
        struct task_desc *next = task_dequeue(&kst->rdy_queues[i]);
        if (next != NULL)
            return next;
    }
    return NULL;
}

int
main()
{
    int i;
    struct kern_state kst;

    bwsetfifo(COM2, OFF);
    *EXC_VEC_SWI = EXC_VEC_INSTR;
    EXC_VEC_FP(EXC_VEC_SWI) = &kern_entry_swi;

    /* Set up kernel state and create initial user task */
    kst_init(&kst);
    task_create(&kst, -1, 5, &u_init_main);
    task_create(&kst, -1, 5, &u_init_main);

    for (i = 0; i < 100; i++) {
        struct task_desc *curtask = task_schedule(&kst);
        if (curtask == NULL) {
            bwprintf(COM2, "no task to schedule, stopping\n");
            break;
        }
        bwprintf(COM2, "scheduled task %d at 0x%x\n", curtask->tid, curtask);

        curtask->state = TASK_STATE_ACTIVE;
        uint32_t ev = ctx_switch(curtask);

        /* print some debug info */
        const char *mode = cpumode_name(cur_cpumode());
        bwprintf(COM2, "%s event = %d\n", mode, ev);
        if (ev == INTR_SWI) {
            uint32_t syscall_no = *((uint32_t*)curtask->regs->pc - 1);
            syscall_no &= 0x00ffffff;
            bwprintf(COM2, "%s swi#  = 0x%x\n", mode, syscall_no);
        }
        bwprintf(COM2, "%s usrsp = 0x%x\n", mode, curtask->regs + 1);

        /* re-enable task */
        curtask->state = TASK_STATE_READY;
        task_enqueue(curtask, &kst.rdy_queues[curtask->priority]);
    }

    return 0;
}
