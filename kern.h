#ifdef KERN_H
#error "double-included kern.h"
#endif

#define KERN_H

XINT_H;
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

/* Kernel entry point */
int kern_main(void);

/* Initialize kernel. */
void kern_init(struct kern *k);

/* Handle an interrupt. */
void kern_handle_intr(struct kern *k, struct task_desc *active, uint32_t intr);

/* Handle a system call. */
void kern_handle_swi(struct kern *k, struct task_desc *active);
