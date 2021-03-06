/*******************************************************************************
    Copyright 2014 Matthew Thiffault

    This file is part of HeatheRTOS.

    HeatheRTOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    HeatheRTOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HeatheRTOS.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************/

#include "config.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"

#include "xbool.h"
#include "xassert.h"
#include "bithack.h"

#include "event.h"
#include "kern.h"
#include "cpumode.h"
#include "u_syscall.h"

/* Get a pointer to the task descriptor with the specified TID */
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

/* Create a new task, returns the new TID */
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

    /* Guaranteed to succeed from this point: initialize task. */
    ix             = TASK_PTR2IX(kern, td);
    td->parent_ix  = parent_ix;
    TASK_SET_PRIO(td, priority); /* task_ready() will set state */

    stack          = kern->user_stacks_bottom - ix * kern->user_stack_size;
    td->regs       = (struct task_regs*)stack - 1; /* leave room for regs */
    td->regs->spsr = cpumode_bits(MODE_USR);       /* interrupts enabled */
    td->regs->sp   = (uint32_t)stack;
    td->regs->lr   = (uint32_t)&Exit; /* call Exit on return of task_entry */
    td->regs->pc   = (uint32_t)task_entry;
    td->cleanup    = NULL;
    td->irq        = (int8_t)-1;
    td->time       = 0;

    taskq_init(&td->senders);

    task_ready(kern, td);
    return TASK_TID(kern, td);
}

/* Put a task on the appropriate kernel ready queue */
void
task_ready(struct kern *kern, struct task_desc *td)
{
    int prio = TASK_PRIO(td);
    TASK_SET_STATE(td, TASK_STATE_READY);
    task_enqueue(kern, td, &kern->rdy_queues[prio]);
    kern->rdy_queue_ne |= 1 << prio;
    kern->rdy_count++;
}

/* Pops the task with the highest priority from its
   ready queue */
/* NB. lower numbers are higher priority! */
struct task_desc*
task_schedule(struct kern *kern)
{
    int prio;
    struct task_queue *q;
    struct task_desc  *td;

    /* There must always be ready tasks */
    assert(kern->rdy_count > 0);
    assert(kern->rdy_queue_ne != 0);

    /* Find the highest priority at which tasks are ready. */
    prio = ctz16(kern->rdy_queue_ne);
    q    = &kern->rdy_queues[prio];
    td   = task_dequeue(kern, q);
    assert(td != NULL); /* if not, rdy_queue_ne was inconsistent */

    if (q->head_ix == TASK_IX_NULL)
        kern->rdy_queue_ne &= ~(1 << prio);

    assert(TASK_STATE(td) == TASK_STATE_READY);
    TASK_SET_STATE(td, TASK_STATE_ACTIVE);
    kern->rdy_count--;
    return td;
}

/* Return a task descriptor to the free list */
void
task_free(struct kern *kern, struct task_desc *td)
{
    TASK_SET_STATE(td, TASK_STATE_FREE);
    td->tid_seq++;
    if (td->irq >= 0) {
        int rc;
        rc = evt_unregister(&kern->eventab, td->irq);
        assertv(rc, rc == 0);
    }
    task_enqueue(kern, td, &kern->free_tasks);
}

/* Initialize a task queue */
void
taskq_init(struct task_queue *q)
{
    q->head_ix = TASK_IX_NULL;
    q->tail_ix = TASK_IX_NULL;
}

/* Enqueue a task */
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

/* Dequeue a task */
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
