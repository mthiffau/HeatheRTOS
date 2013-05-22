#ifdef KERN_H
#error "double-included kern.h"
#endif

#define KERN_H

XINT_H;

#define MAX_TASKS           128  /* must be <= 255 */
#define N_PRIORITIES        16

XDEF_H;
TASK_H;

struct kern {
    void             *stack_mem_top;
    size_t            stack_size;
    int               ntasks;
    struct task_desc  tasks[MAX_TASKS];
    uint16_t          rdy_queue_ne; /* bit i set if queue i nonempty */
    struct task_queue rdy_queues[N_PRIORITIES];
    struct task_queue free_tasks;
};

/* Initialize kernel. */
void kern_init(struct kern *k);

/* Handle an interrupt. */
void kern_handle_intr(struct kern *k, struct task_desc *active, uint32_t intr);

/* Handle a system call. */
void kern_handle_swi(struct kern *k, struct task_desc *active);

/* Create a new task. Returns the TID of the newly created task,
 * or an error code: -1 for invalid priority, -2 if out of task
 * descriptors.
 *
 * Valid priorities are 0 <= p < N_PRIORITIES, where lower numbers
 * indicate higher priority. */
tid_t task_create(
    struct kern *k,
    uint8_t parent_ix,
    int priority,
    void (*task_entry)(void));

/* Add a task to the ready queue for its priority. */
void task_ready(struct kern *k, struct task_desc *td);

/* Schedule the highest priority ready task.
 * The scheduled task is marked active and removed from ready queue.
 * If no tasks are ready, returns NULL. */
struct task_desc *task_schedule(struct kern *k);
