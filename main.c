#include <bwio.h>
#include <ts7200.h>

#define PSR_MODE_MASK 0x1F

#define EXC_VEC_INSTR       0xe59ff018 /* ldr pc, [pc, #+0x18] */
#define EXC_VEC_SWI         ((unsigned int*)0x8)
#define EXC_VEC_FP(i)       (*((void**)((void*)(i) + 0x20)))

enum {
    MODE_USR=0x10,
    MODE_FIQ=0x11,
    MODE_IRQ=0x12,
    MODE_SVC=0x13,
    MODE_ABT=0x17,
    MODE_UND=0x1B,
    MODE_SYS=0x1F
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
    return get_cpsr() & PSR_MODE_MASK;
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
    bwprintf(COM2, "%x\n", cpu_mode());
}

void
swi_handler(void)
{
    sub();
}

int
main()
{
    *EXC_VEC_SWI = EXC_VEC_INSTR;
    EXC_VEC_FP(EXC_VEC_SWI) = &swi_handler;

    bwsetfifo(COM2, OFF);
    sub();
    swi_test();

    return 0;
}
