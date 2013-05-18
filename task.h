#ifdef TASK_H
#error "double-included task.h"
#endif

#define TASK_H

STATIC_ASSERT_H;

struct task_state;

struct task_desc {
    /* Points into the task's stack. The task's stack pointer is
     * state + sizeof (task_state). */
    struct task_state *state;
};

struct task_state {
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
STATIC_ASSERT(task_state_spsr, offsetof (struct task_state, spsr) == 0x0);
STATIC_ASSERT(task_state_r0,   offsetof (struct task_state, r0)   == 0x4);
STATIC_ASSERT(task_state_r1,   offsetof (struct task_state, r1)   == 0x8);
STATIC_ASSERT(task_state_r2,   offsetof (struct task_state, r2)   == 0xc);
STATIC_ASSERT(task_state_r3,   offsetof (struct task_state, r3)   == 0x10);
STATIC_ASSERT(task_state_r4,   offsetof (struct task_state, r4)   == 0x14);
STATIC_ASSERT(task_state_r5,   offsetof (struct task_state, r5)   == 0x18);
STATIC_ASSERT(task_state_r6,   offsetof (struct task_state, r6)   == 0x1c);
STATIC_ASSERT(task_state_r7,   offsetof (struct task_state, r7)   == 0x20);
STATIC_ASSERT(task_state_r8,   offsetof (struct task_state, r8)   == 0x24);
STATIC_ASSERT(task_state_r9,   offsetof (struct task_state, r9)   == 0x28);
STATIC_ASSERT(task_state_r10,  offsetof (struct task_state, r10)  == 0x2c);
STATIC_ASSERT(task_state_r11,  offsetof (struct task_state, r11)  == 0x30);
STATIC_ASSERT(task_state_r12,  offsetof (struct task_state, r12)  == 0x34);
STATIC_ASSERT(task_state_lr,   offsetof (struct task_state, lr)   == 0x38);
STATIC_ASSERT(task_state_pc,   offsetof (struct task_state, pc)   == 0x3c);
STATIC_ASSERT(task_state_size, sizeof   (struct task_state)       == 0x40);
