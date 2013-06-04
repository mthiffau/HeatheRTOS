#ifdef CPUMODE_H
#error "double-included cpumode.h"
#endif

#define CPUMODE_H

XINT_H;

typedef uint8_t cpumode_t;

#define DEFMODE(name, bits) MODE_##name,
enum {
#include "cpumode.list"
};
#undef DEFMODE

uint32_t    cur_cpsr(void);
cpumode_t   cur_cpumode(void);
uint32_t    cpumode_bits(cpumode_t);
cpumode_t   cpumode_from_bits(uint32_t);
const char *cpumode_name(cpumode_t);
