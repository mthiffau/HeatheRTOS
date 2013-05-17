#include "xint.h"
#include "bwio.h"
#include "ts7200.h"
#include "xsetjmp.h"

#define PSR_MODE_MASK 0x1F

#define EXC_VEC_INSTR       0xe59ff018 /* ldr pc, [pc, #+0x18] */
#define EXC_VEC_SWI         ((unsigned int*)0x8)
#define EXC_VEC_FP(i)       (*((void**)((void*)(i) + 0x20)))

#define TASK_SP_SPSR_IX     0
#define TASK_SP_R0_IX       1
#define TASK_SP_PC_IX       15
#define TASK_SP_REGSAV_LEN  0x40

struct taskdesc {
    uint32_t *sp;
};

struct event {
    int syscall;
};

void ctxswitch(struct taskdesc *td, struct event *ev);

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
    struct taskdesc curtask;
    struct event ev;

    *EXC_VEC_SWI = EXC_VEC_INSTR;
    EXC_VEC_FP(EXC_VEC_SWI) = &kern_entry_swi;
    bwsetfifo(COM2, OFF);

    /* Set up task */
    curtask.sp = (uint32_t*)(0x01000000 - TASK_SP_REGSAV_LEN);
    curtask.sp[TASK_SP_SPSR_IX] = 0x10; /* user mode, interrupts enabled */
    curtask.sp[TASK_SP_PC_IX]   = (unsigned int)&task_main;

    for (i = 0; i < 10; i++) {
        ctxswitch(&curtask, &ev);
        bwprintf(COM2, "after ctxswitch(%s) %d\n", mode_names[cpu_mode()], i);
    }

    bwprintf(COM2, "exited\n");

    return 0;
}
