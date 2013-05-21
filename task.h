#ifdef TASK_H
#error "double-included task.h"
#endif

#define TASK_H

XINT_H;
U_TID_H; /* for tid_t */
STATIC_ASSERT_H;

struct task_regs;

enum {
    TASK_STATE_READY,               /* Ready to run; is in ready queue */
    TASK_STATE_ACTIVE,              /* 'Currently' running. */
    TASK_STATE_ZOMBIE,              /* Exited. */
    TASK_STATE_SEND_BLOCKED,        /* Blocked in Receive, waiting for Send. */
    TASK_STATE_RECEIVE_BLOCKED,     /* Blocked in Send, waiting for Receive. */
    TASK_STATE_REPLY_BLOCKED,       /* Blocked in Send, waiting for Reply. */
    TASK_STATE_EVENT_BLOCKED        /* Blocked in AwaitEvent */
};

struct task_desc {
    /* Points into the task's stack. The task's stack pointer is
     * state + sizeof (task_regs). Context switch assumes this is
     * the first member of struct task_desc. */
    struct task_regs *regs;

    /* Task info */
    tid_t tid;         /* invalid if this entry is not in use */
    tid_t parent_tid;
    uint8_t priority;
    uint8_t state;
    /* NB. no spsr         - use regs->spsr
     *     no return value - use regs->r0. */

    /* Participation in ready queue or other list as per state.
     *  - ready queue: doubly-linked
     *  - free list:   singly-linked */

    struct task_desc *next;
    struct task_desc *prev;
};

/* Context switch assumes this memory layout */
struct task_regs {
    uint32_t spsr;
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
    /* no sp */
    uint32_t lr;
    uint32_t pc;
};

/* Check memory layout is as assumed by context switch */
STATIC_ASSERT(task_regs_spsr, offsetof (struct task_regs, spsr) == 0x0);
STATIC_ASSERT(task_regs_r0,   offsetof (struct task_regs, r0)   == 0x4);
STATIC_ASSERT(task_regs_r1,   offsetof (struct task_regs, r1)   == 0x8);
STATIC_ASSERT(task_regs_r2,   offsetof (struct task_regs, r2)   == 0xc);
STATIC_ASSERT(task_regs_r3,   offsetof (struct task_regs, r3)   == 0x10);
STATIC_ASSERT(task_regs_r4,   offsetof (struct task_regs, r4)   == 0x14);
STATIC_ASSERT(task_regs_r5,   offsetof (struct task_regs, r5)   == 0x18);
STATIC_ASSERT(task_regs_r6,   offsetof (struct task_regs, r6)   == 0x1c);
STATIC_ASSERT(task_regs_r7,   offsetof (struct task_regs, r7)   == 0x20);
STATIC_ASSERT(task_regs_r8,   offsetof (struct task_regs, r8)   == 0x24);
STATIC_ASSERT(task_regs_r9,   offsetof (struct task_regs, r9)   == 0x28);
STATIC_ASSERT(task_regs_r10,  offsetof (struct task_regs, r10)  == 0x2c);
STATIC_ASSERT(task_regs_r11,  offsetof (struct task_regs, r11)  == 0x30);
STATIC_ASSERT(task_regs_r12,  offsetof (struct task_regs, r12)  == 0x34);
STATIC_ASSERT(task_regs_lr,   offsetof (struct task_regs, lr)   == 0x38);
STATIC_ASSERT(task_regs_pc,   offsetof (struct task_regs, pc)   == 0x3c);
STATIC_ASSERT(task_regs_size, sizeof   (struct task_regs)       == 0x40);
