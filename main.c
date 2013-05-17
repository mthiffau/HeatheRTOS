#include "xint.h"
#include "bwio.h"
#include "ts7200.h"
#include "xsetjmp.h"
#include "xdef.h"
#include "static_assert.h"

#define PSR_MODE_MASK 0x1F

#define EXC_VEC_INSTR       0xe59ff018 /* ldr pc, [pc, #+0x18] */
#define EXC_VEC_SWI         ((unsigned int*)0x8)
#define EXC_VEC_FP(i)       (*((void**)((void*)(i) + 0x20)))

#define TASK_SP_SPSR_IX     0
#define TASK_SP_R0_IX       1
#define TASK_SP_PC_IX       15
#define TASK_SP_REGSAV_LEN  0x40

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

struct task_desc {
    /* Points into the task's stack. The task's stack pointer is
     * state + sizeof (task_state). */
    struct task_state *state;
};

struct event {
    uint32_t ev;
    uint32_t args[5];
};

void ctx_switch(struct task_desc *td, struct event *ev);

typedef uint8_t cpumode_t;

#define DEFMODE(name, bits) MODE_##name,
enum {
#include "modes.inc"
};
#undef DEFMODE

#define DEFMODE(name, bits) bits,
static const unsigned int mode_bits[] = {
#include "modes.inc"
};
#undef DEFMODE

#define DEFMODE(name, bits) #name,
static const char * const mode_names[] = {
#include "modes.inc"
};
#undef DEFMODE

static const cpumode_t mode_from_bits[] = {
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    MODE_USR,
    MODE_FIQ,
    MODE_IRQ,
    MODE_SVC,
    -1,
    -1,
    -1,
    MODE_ABT,
    -1,
    -1,
    -1,
    MODE_UND,
    -1,
    -1,
    -1,
    MODE_SYS
};

void
trigger_swi(void);

void
kern_entry_swi(void);

unsigned int
get_cpsr(void)
{
    unsigned int cpsr;
    asm("mrs %0, cpsr\n"
        : "=r"(cpsr)
        /* : No input */
        /* : No Clobber */
        );
    return cpsr;
}

unsigned int
cpu_mode(void)
{
    return mode_from_bits[get_cpsr() & PSR_MODE_MASK];
}

void
task_inner(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        bwprintf(COM2, "task_inner %d\n", i);
        trigger_swi();
    }
}

void
task_main(void)
{
    int i = 0;
    bwputstr(COM2, "task_main start\n");
    for (;;) {
        char c = bwgetc(COM2);
        bwprintf(COM2, "task_main(%s) %d %c\n", mode_names[cpu_mode()], i++, c);
        trigger_swi();
        task_inner();
    }
}

int
main()
{
    int i;
    struct task_desc curtask;
    struct event ev;

    *EXC_VEC_SWI = EXC_VEC_INSTR;
    EXC_VEC_FP(EXC_VEC_SWI) = &kern_entry_swi;
    bwsetfifo(COM2, OFF);

    /* Set up task */
    curtask.state = (struct task_state*)0x01000000 - 1;
    curtask.state->spsr = 0x10; /* user mode, all interrupts enabled */
    curtask.state->pc   = (uint32_t)&task_main;

    for (i = 0; i < 10; i++) {
        ctx_switch(&curtask, &ev);
        bwprintf(COM2, "after ctx_switch(%s) %d\n", mode_names[cpu_mode()], i);
    }

    bwprintf(COM2, "exited\n");

    return 0;
}
