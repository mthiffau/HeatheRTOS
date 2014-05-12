#ifndef TASK_H
#define TASK_H

#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "config.h"

struct kern;
struct task_regs;
struct task_fpu_regs;

/* Conversion to/from  */
#define TASK_IX2PTR(kern, tix) (&(kern)->tasks[(tix)])
#define TASK_PTR2IX(kern, tdp) ((tdp) - (kern)->tasks)
#define TASK_IX_NULL           0xff  /* invalid index value */
#define TASK_IX_NOTINQUEUE     0xfe  /* invalid index value */

/* Task ID is 16 bits:
 * - low byte is task descriptor index, high byte is a sequence number */
#define TID_SEQ_OFFS    8
#define TID_IX_MASK     0xff
#define TASK_TID(kern, tdp)    \
    (((tdp)->tid_seq << TID_SEQ_OFFS) | TASK_PTR2IX(kern, tdp))

/* Task descriptor stores state and priority in a single byte */
#define TASK_STATE_MASK 0xf0
#define TASK_PRIO_MASK  0x0f
#define TASK_STATE(tdp) ((tdp)->state_prio & TASK_STATE_MASK)
#define TASK_PRIO(tdp)  ((tdp)->state_prio & TASK_PRIO_MASK)
#define TASK_SET_STATE(tdp, state) \
    ((tdp)->state_prio = ((tdp)->state_prio & ~TASK_STATE_MASK) | (state))
#define TASK_SET_PRIO(tdp, prio)   \
    ((tdp)->state_prio = ((tdp)->state_prio & ~TASK_PRIO_MASK) | (prio))

/* Task state constants */
enum {
    TASK_STATE_FREE            = 0x00, /* Unused task descriptor */
    TASK_STATE_READY           = 0x10, /* Ready to run; is in ready queue */
    TASK_STATE_ACTIVE          = 0x20, /* 'Currently' running */
    TASK_STATE_ZOMBIE          = 0x30, /* Exited */
    TASK_STATE_SEND_BLOCKED    = 0x40, /* Blocked: Receive waiting for Send */
    TASK_STATE_RECEIVE_BLOCKED = 0x50, /* Blocked: Send waiting for Receive */
    TASK_STATE_REPLY_BLOCKED   = 0x60, /* Blocked: Send waiting for Reply */
    TASK_STATE_EVENT_BLOCKED   = 0x70  /* Blocked: AwaitEvent */
};

/* Singly-linked task queue */
struct task_queue {
    uint8_t head_ix;
    uint8_t tail_ix;
};
STATIC_ASSERT(task_queue_size, sizeof (struct task_queue) == 2);

struct task_desc {
    /* Points into the task's stack. The task's stack pointer is
     * state + sizeof (task_regs). Context switch assumes this is
     * the first member of struct task_desc. */
    volatile struct task_regs *regs;

    /* Task info */
    uint8_t state_prio; /* sssspppp : s state, p priority */
    uint8_t tid_seq;    /* high byte of tid */
    uint8_t parent_ix;  /* parent task descriptor index */
    uint8_t next_ix;    /* next pointer task descriptor index */
    /* NB. no spsr         - use regs->spsr
     *     no return value - use regs->r0. */

    /* Send queue for this task */
    struct task_queue senders;

    /* Registered cleanup function */
    void (*cleanup)(void);

    /* Registered event (IRQ number) */
    int8_t irq;

    /* Time spent in task. */
    uint32_t time;

    /* Flag which is set to true if the task has a floating
     point context saved on it's stack. */
    uint8_t fpu_ctx_on_stack;

    /* Points to FPU Context on stack, if not null. */
    volatile struct task_fpu_regs *fpu_regs;
};
STATIC_ASSERT(task_desc_size, sizeof (struct task_desc) == 32);


/* Context switch assumes this memory layout */
struct task_regs {
    uint32_t spsr;
    uint32_t pc;
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t r12;
    uint32_t sp;
    uint32_t lr;
};

/* Check memory layout is as assumed by context switch */
STATIC_ASSERT(task_regs_spsr, offsetof (struct task_regs, spsr) == 0x0);
STATIC_ASSERT(task_regs_pc,   offsetof (struct task_regs, pc)   == 0x4);
STATIC_ASSERT(task_regs_r0,   offsetof (struct task_regs, r0)   == 0x8);
STATIC_ASSERT(task_regs_r1,   offsetof (struct task_regs, r1)   == 0xc);
STATIC_ASSERT(task_regs_r2,   offsetof (struct task_regs, r2)   == 0x10);
STATIC_ASSERT(task_regs_r3,   offsetof (struct task_regs, r3)   == 0x14);
STATIC_ASSERT(task_regs_r4,   offsetof (struct task_regs, r4)   == 0x18);
STATIC_ASSERT(task_regs_r5,   offsetof (struct task_regs, r5)   == 0x1c);
STATIC_ASSERT(task_regs_r6,   offsetof (struct task_regs, r6)   == 0x20);
STATIC_ASSERT(task_regs_r7,   offsetof (struct task_regs, r7)   == 0x24);
STATIC_ASSERT(task_regs_r8,   offsetof (struct task_regs, r8)   == 0x28);
STATIC_ASSERT(task_regs_r9,   offsetof (struct task_regs, r9)   == 0x2c);
STATIC_ASSERT(task_regs_r10,  offsetof (struct task_regs, r10)  == 0x30);
STATIC_ASSERT(task_regs_r11,  offsetof (struct task_regs, r11)  == 0x34);
STATIC_ASSERT(task_regs_r12,  offsetof (struct task_regs, r12)  == 0x38);
STATIC_ASSERT(task_regs_sp,   offsetof (struct task_regs, sp)   == 0x3c);
STATIC_ASSERT(task_regs_lr,   offsetof (struct task_regs, lr)   == 0x40);
STATIC_ASSERT(task_regs_size, sizeof   (struct task_regs)       == 0x44);

/* Context switch assumes this memory layout */
struct task_fpu_regs {
    uint32_t FPSCR;
    uint32_t D0_low;
    uint32_t D0_high;
    uint32_t D1_low;
    uint32_t D1_high;
    uint32_t D2_low;
    uint32_t D2_high;
    uint32_t D3_low;
    uint32_t D3_high;
    uint32_t D4_low;
    uint32_t D4_high;
    uint32_t D5_low;
    uint32_t D5_high;
    uint32_t D6_low;
    uint32_t D6_high;
    uint32_t D7_low;
    uint32_t D7_high;
    uint32_t D8_low;
    uint32_t D8_high;
    uint32_t D9_low;
    uint32_t D9_high;
    uint32_t D10_low;
    uint32_t D10_high;
    uint32_t D11_low;
    uint32_t D11_high;
    uint32_t D12_low;
    uint32_t D12_high;
    uint32_t D13_low;
    uint32_t D13_high;
    uint32_t D14_low;
    uint32_t D14_high;
    uint32_t D15_low;
    uint32_t D15_high;
    uint32_t D16_low;
    uint32_t D16_high;
    uint32_t D17_low;
    uint32_t D17_high;
    uint32_t D18_low;
    uint32_t D18_high;
    uint32_t D19_low;
    uint32_t D19_high;
    uint32_t D20_low;
    uint32_t D20_high;
    uint32_t D21_low;
    uint32_t D21_high;
    uint32_t D22_low;
    uint32_t D22_high;
    uint32_t D23_low;
    uint32_t D23_high;
    uint32_t D24_low;
    uint32_t D24_high;
    uint32_t D25_low;
    uint32_t D25_high;
    uint32_t D26_low;
    uint32_t D26_high;
    uint32_t D27_low;
    uint32_t D27_high;
    uint32_t D28_low;
    uint32_t D28_high;
    uint32_t D29_low;
    uint32_t D29_high;
    uint32_t D30_low;
    uint32_t D30_high;
    uint32_t D31_low;
    uint32_t D31_high;
};

/* Check memory layout is as assumed by context switch */
STATIC_ASSERT(task_fpu_regs_fpscr, offsetof (struct task_fpu_regs, FPSCR)   == 0x0);
STATIC_ASSERT(task_fpu_regs_d0,    offsetof (struct task_fpu_regs, D0_low)  == 0x4);
STATIC_ASSERT(task_fpu_regs_d1,    offsetof (struct task_fpu_regs, D1_low)  == 0xC);
STATIC_ASSERT(task_fpu_regs_d2,    offsetof (struct task_fpu_regs, D2_low)  == 0x14);
STATIC_ASSERT(task_fpu_regs_d3,    offsetof (struct task_fpu_regs, D3_low)  == 0x1C);
STATIC_ASSERT(task_fpu_regs_d4,    offsetof (struct task_fpu_regs, D4_low)  == 0x24);
STATIC_ASSERT(task_fpu_regs_d5,    offsetof (struct task_fpu_regs, D5_low)  == 0x2C);
STATIC_ASSERT(task_fpu_regs_d6,    offsetof (struct task_fpu_regs, D6_low)  == 0x34);
STATIC_ASSERT(task_fpu_regs_d7,    offsetof (struct task_fpu_regs, D7_low)  == 0x3C);
STATIC_ASSERT(task_fpu_regs_d8,    offsetof (struct task_fpu_regs, D8_low)  == 0x44);
STATIC_ASSERT(task_fpu_regs_d9,    offsetof (struct task_fpu_regs, D9_low)  == 0x4c);
STATIC_ASSERT(task_fpu_regs_d10,   offsetof (struct task_fpu_regs, D10_low) == 0x54);
STATIC_ASSERT(task_fpu_regs_d11,   offsetof (struct task_fpu_regs, D11_low) == 0x5c);
STATIC_ASSERT(task_fpu_regs_d12,   offsetof (struct task_fpu_regs, D12_low) == 0x64);
STATIC_ASSERT(task_fpu_regs_d13,   offsetof (struct task_fpu_regs, D13_low) == 0x6c);
STATIC_ASSERT(task_fpu_regs_d14,   offsetof (struct task_fpu_regs, D14_low) == 0x74);
STATIC_ASSERT(task_fpu_regs_d15,   offsetof (struct task_fpu_regs, D15_low) == 0x7c);
STATIC_ASSERT(task_fpu_regs_d16,   offsetof (struct task_fpu_regs, D16_low) == 0x84);
STATIC_ASSERT(task_fpu_regs_d17,   offsetof (struct task_fpu_regs, D17_low) == 0x8c);
STATIC_ASSERT(task_fpu_regs_d18,   offsetof (struct task_fpu_regs, D18_low) == 0x94);
STATIC_ASSERT(task_fpu_regs_d19,   offsetof (struct task_fpu_regs, D19_low) == 0x9c);
STATIC_ASSERT(task_fpu_regs_d20,   offsetof (struct task_fpu_regs, D20_low) == 0xa4);
STATIC_ASSERT(task_fpu_regs_d21,   offsetof (struct task_fpu_regs, D21_low) == 0xac);
STATIC_ASSERT(task_fpu_regs_d22,   offsetof (struct task_fpu_regs, D22_low) == 0xb4);
STATIC_ASSERT(task_fpu_regs_d23,   offsetof (struct task_fpu_regs, D23_low) == 0xbc);
STATIC_ASSERT(task_fpu_regs_d24,   offsetof (struct task_fpu_regs, D24_low) == 0xc4);
STATIC_ASSERT(task_fpu_regs_d25,   offsetof (struct task_fpu_regs, D25_low) == 0xcc);
STATIC_ASSERT(task_fpu_regs_d26,   offsetof (struct task_fpu_regs, D26_low) == 0xd4);
STATIC_ASSERT(task_fpu_regs_d27,   offsetof (struct task_fpu_regs, D27_low) == 0xdc);
STATIC_ASSERT(task_fpu_regs_d28,   offsetof (struct task_fpu_regs, D28_low) == 0xe4);
STATIC_ASSERT(task_fpu_regs_d29,   offsetof (struct task_fpu_regs, D29_low) == 0xec);
STATIC_ASSERT(task_fpu_regs_d30,   offsetof (struct task_fpu_regs, D30_low) == 0xf4);
STATIC_ASSERT(task_fpu_regs_d31,   offsetof (struct task_fpu_regs, D31_low) == 0xfc);

STATIC_ASSERT(task_fpu_regs_size, sizeof   (struct task_fpu_regs)       == 0x104);

/* Find a task by its TID. Returns one of the following error codes. */
int get_task(struct kern *k, tid_t tid, struct task_desc **td_out);

enum {
    GET_TASK_SUCCESS        =  0,
    GET_TASK_IMPOSSIBLE_TID = -1,
    GET_TASK_NO_SUCH_TASK   = -2,
};

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

/* Initialize a task queue to be empty. */
void taskq_init(struct task_queue *q);

/* Free a task descriptor so that it can be reused later. */
void task_free(struct kern *k, struct task_desc *td);

/* Enqueue a task on a task queue.
 * A task can only be on one task queue at once.
 * Do not enqueue tasks that are still part of another queue! */
void task_enqueue(struct kern*, struct task_desc*, struct task_queue*);

/* Attempt to dequeue a task from a task queue. Returns NULL if empty. */
struct task_desc *task_dequeue(struct kern*, struct task_queue*);

#endif
