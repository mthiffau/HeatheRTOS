#ifdef TASK_H
#error "double-included task.h"
#endif

#define TASK_H

XINT_H;
STATIC_ASSERT_H;

struct task_regs;

/* Conversion to/from  */
#define TASK_IX2PTR(kern, tix) (&(kern)->tasks[(tix)])
#define TASK_PTR2IX(kern, tdp) ((tdp) - (kern)->tasks)
#define TASK_IX_NULL           0xff  /* invalid index value */

/* Task ID is 16 bits:
 * - low byte is task descriptor index, high byte is a sequence number */
#define TID_SEQ_OFFS    8
#define TID_IX_MASK     0xff
#define TASK_TID(kern, tdp)    \
    ((tdp)->tid_seq << TID_SEQ_OFFS) | TASK_PTR2IX(kern, tdp)

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
STATIC_ASSERT(task_queue_size, sizeof (struct task_queue) == 4);

struct task_desc {
    /* Points into the task's stack. The task's stack pointer is
     * state + sizeof (task_regs). Context switch assumes this is
     * the first member of struct task_desc. */
    struct task_regs *regs;

    /* Task info */
    uint8_t state_prio; /* sssspppp : s state, p priority */
    uint8_t tid_seq;    /* high byte of tid */
    uint8_t parent_ix;  /* parent task descriptor index */
    uint8_t next_ix;    /* next pointer task descriptor index */
    /* NB. no spsr         - use regs->spsr
     *     no return value - use regs->r0. */
};
STATIC_ASSERT(task_desc_size, sizeof (struct task_desc) == 8);

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
