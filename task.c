#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"

#include "xbool.h"
#include "xassert.h"

#include "kern.h"
#include "cpumode.h"
#include "u_syscall.h"

static inline int ctz16(uint16_t x);

int
get_task(struct kern *kern, tid_t tid, struct task_desc **td_out)
{
    struct task_desc *td;
    int ix;

    ix = tid & 0xff;
    if (ix < 0 || ix >= MAX_TASKS || (tid & ~0xffff) != 0)
        return GET_TASK_IMPOSSIBLE_TID;

    td = &kern->tasks[ix];
    if (TASK_STATE(td) == TASK_STATE_FREE || td->tid_seq != (tid >> 8))
        return GET_TASK_NO_SUCH_TASK;

    *td_out = td;
    return GET_TASK_SUCCESS;
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

    taskq_init(&td->senders);

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

void
task_free(struct kern *kern, struct task_desc *td)
{
    TASK_SET_STATE(td, TASK_STATE_FREE);
    td->tid_seq++;
    task_enqueue(kern, td, &kern->free_tasks);
    kern->ntasks--;
}

void
taskq_init(struct task_queue *q)
{
    q->head_ix = TASK_IX_NULL;
    q->tail_ix = TASK_IX_NULL;
}

void
task_enqueue(struct kern *kern, struct task_desc *td, struct task_queue *q)
{
    uint8_t ix = TASK_PTR2IX(kern, td);
    assert(td->next_ix == TASK_IX_NOTINQUEUE);
    td->next_ix = TASK_IX_NULL;
    if (q->head_ix == TASK_IX_NULL) {
        q->head_ix = ix;
        q->tail_ix = ix;
    } else {
        TASK_IX2PTR(kern, q->tail_ix)->next_ix = ix;
        q->tail_ix = ix;
    }
}

struct task_desc*
task_dequeue(struct kern *kern, struct task_queue *q)
{
    struct task_desc *td;
    if (q->head_ix == TASK_IX_NULL)
        return NULL;

    td = TASK_IX2PTR(kern, q->head_ix);
    q->head_ix = td->next_ix;
    td->next_ix = TASK_IX_NOTINQUEUE;
    return td;
}

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

