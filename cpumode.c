#include "xint.h"
#include "cpumode.h"

/* Mask for the bits of the PSR
   which represent the mode */
#define PSR_MODE_MASK 0x1F

/* Macro magic to load the CPU
   mode definitions */
#define DEFMODE(name, bits) bits,
static const unsigned int mode_bits[] = {
#include "cpumode.list"
};
#undef DEFMODE

#define DEFMODE(name, bits) #name,
static const char * const mode_name[] = {
#include "cpumode.list"
};
#undef DEFMODE

static const cpumode_t bits_mode[] = {
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

/* Get the current value of the CPSR */
uint32_t
cur_cpsr(void)
{
    unsigned int cpsr;
    asm("mrs %0, cpsr\n"
        : "=r"(cpsr)
        /* : No input */
        /* : No Clobber */
        );
    return cpsr;
}

/* Get the current CPU mode */
cpumode_t
cur_cpumode(void)
{
    return bits_mode[cur_cpsr() & PSR_MODE_MASK];
}

/* CPU Mode Helper Functions */
uint32_t
cpumode_bits(cpumode_t m)
{
    return mode_bits[m];
}

cpumode_t
cpumode_from_bits(uint32_t bits)
{
    return bits_mode[bits & 0x1f];
}

const char *
cpumode_name(cpumode_t m)
{
    return mode_name[m];
}
