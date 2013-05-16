#include "xint.h"
#include "bwio.h"
#include "ts7200.h"
#include "xsetjmp.h"

#define PSR_MODE_MASK 0x1F

#define EXC_VEC_INSTR       0xe59ff018 /* ldr pc, [pc, #+0x18] */
#define EXC_VEC_SWI         ((unsigned int*)0x8)
#define EXC_VEC_FP(i)       (*((void**)((void*)(i) + 0x20)))

struct taskdesc {
    unsigned int r[16];
    unsigned int psr;
} only_task;

struct taskdesc *curtask = &only_task;

void activate_ctx(struct taskdesc *td) __attribute__((noreturn));

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
swi_test(void);

void
exch_swi(void);

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
set_cpu_mode(unsigned int mode)
{
    mode &= PSR_MODE_MASK;
    asm("mrs ip, cpsr\n\t"
        "bic ip, ip, #31\n\t"
        "orr ip, ip, %0\n\t"
        "msr cpsr_c, ip\n\t"
        :           /* no output */
        : "r"(mode) /* input */
        : "ip"      /* clobber */
        );
}

void
sub(void)
{
    bwprintf(COM2, "%s\n", mode_names[cpu_mode()]);
}

jmp_buf kern_exit;

void
infinite(void)
{
    for (;;) {
        bwgetc(COM2);
        longjmp(kern_exit, 1);
    }
}

void
task_main(void)
{
    for (;;) {
        char c = bwgetc(COM2);
        bwprintf(COM2, "task_main(%s) %c\n", mode_names[cpu_mode()], c);
        swi_test();
    }
}

void kern_event(uint32_t swi) __attribute__((noreturn));

int
main()
{
    int foobar;

    *EXC_VEC_SWI = EXC_VEC_INSTR;
    EXC_VEC_FP(EXC_VEC_SWI) = &exch_swi;
    bwsetfifo(COM2, OFF);

    bwprintf(COM2, "%x\n", &foobar);

    /* Set up task */
    only_task.r[13] = 0x1000000; /* stack pointer */
    only_task.r[15] = (unsigned int)&task_main; /* entry point */

    /* longjmp test */
    if (setjmp(kern_exit) == 0) {
        bwprintf(COM2, "starting up\n");
        kern_event(-1);
    }

    bwprintf(COM2, "exited\n");

    return 0;
}

void
kern_event(uint32_t swi)
{
    bwprintf(COM2, "kern_event %s %x\n", mode_names[cpu_mode()], swi);
    activate_ctx(curtask);
}
