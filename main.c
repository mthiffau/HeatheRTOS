#include <bwio.h>
#include <ts7200.h>

#define PSR_MODE_MASK 0x1F

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
sub(void)
{
    return;
}

unsigned int 
determine_mode(void)
{
    unsigned int cpsr;

    asm("MRS %0, CPSR\n"
        : "=r"(cpsr)
        /* : No input */
        /* : No Clobber */
        );
    return cpsr;
}

int 
main() 
{
    bwsetfifo(COM2, OFF);
    bwprintf(COM2,"foobar!\n");
    bwprintf(COM2, "%x\n", determine_mode());
    return 0;
}
