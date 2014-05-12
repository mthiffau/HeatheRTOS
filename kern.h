#ifndef KERN_H
#define KERN_H

#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "task.h"
#include "event.h"

struct kern {
#ifdef HARD_FLOAT
    struct task_desc* fp_ctx_holder;
#endif

    void             *user_stacks_bottom;
    size_t            user_stack_size;
    struct task_desc  tasks[MAX_TASKS];
    uint16_t          rdy_queue_ne; /* bit i set if queue i nonempty */
    struct task_queue rdy_queues[N_PRIORITIES];
    struct task_queue free_tasks;
    struct eventab    eventab;

    /* Termination control. Kernel exits either when there has been a
     * shutdown request, or when no tasks are ready or event-blocked. */
    bool shutdown;
    int  rdy_count;
    int  evblk_count;
};

struct kparam {
    /* Initial user task entry point and priority. */
    void (*init)(void);
    int  init_prio;
    bool show_top; /* print the time taken by each task? */
};

extern struct kparam def_kparam;

/* Kernel entry point */
int kern_main(struct kparam*);

/* Initialize kernel. */
void kern_init(struct kern *k, struct kparam *kp);

/* Handle an interrupt. */
int kern_handle_intr(struct kern *k, struct task_desc *active, uint32_t intr);

/* Handle a system call. */
void kern_handle_swi(struct kern *k, struct task_desc *active);

/* Handle a hardware interrupt. */
void kern_handle_irq(struct kern *k, struct task_desc *active);

/* Handle an undefined instruction */
int kern_handle_undef(struct kern *k, struct task_desc *active);

/* Reset hardware state before returning to RedBoot. */
void kern_cleanup(struct kern *kern);

#endif
