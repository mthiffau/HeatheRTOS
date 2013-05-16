#include "xint.h"
#include "bwio.h"
#include "ts7200.h"
#include "xsetjmp.h"

#define PSR_MODE_MASK 0x1F

#define EXC_VEC_INSTR       0xe59ff018 /* ldr pc, [pc, #+0x18] */
#define EXC_VEC_SWI         ((unsigned int*)0x8)
#define EXC_VEC_FP(i)       (*((void**)((void*)(i) + 0x20)))

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

void
swi_handler(void)
{
    sub();
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

int
main()
{
    *EXC_VEC_SWI = EXC_VEC_INSTR;
    EXC_VEC_FP(EXC_VEC_SWI) = &swi_handler;
    bwsetfifo(COM2, OFF);

    /* longjmp test */
    if (setjmp(kern_exit) == 0) {
        bwprintf(COM2, "starting up\n");
        infinite();
    }

    bwprintf(COM2, "exited\n");

    return 0;
}
