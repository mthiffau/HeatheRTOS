#ifdef KERN_H
#error "double-included kern.h"
#endif

#define KERN_H

XBOOL_H;
XINT_H;
XDEF_H;
TASK_H;
EVENT_H;

struct kern {
    void             *stack_mem_top;
    size_t            stack_size;
    int               ntasks;
    struct task_desc  tasks[MAX_TASKS];
    uint16_t          rdy_queue_ne; /* bit i set if queue i nonempty */
    struct task_queue rdy_queues[N_PRIORITIES];
    struct task_queue free_tasks;
    struct eventab    eventab;
    bool              shutdown;
};

/* TODO: cache, IRQ settings. Maybe a hook run every kernel loop */
struct kparam {
    /* Initial user task entry point and priority. */
    void (*init)(void);
    int  init_prio;
};

extern struct kparam def_kparam;

/* Kernel entry point */
int kern_main(struct kparam*);

/* Initialize kernel. */
void kern_init(struct kern *k, struct kparam *kp);

/* Handle an interrupt. */
void kern_handle_intr(struct kern *k, struct task_desc *active, uint32_t intr);

/* Handle a system call. */
void kern_handle_swi(struct kern *k, struct task_desc *active);

/* Handle a hardware interrupt. */
void kern_handle_irq(struct kern *k, struct task_desc *active);

/* Reset hardware state before returning to RedBoot. */
void kern_cleanup(void);
