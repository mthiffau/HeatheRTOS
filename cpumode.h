#ifndef CPUMODE_H
#define CPUMODE_H

#include "xint.h"

/* Typdef for the CPU mode */
typedef uint8_t cpumode_t;

#define DEFMODE(name, bits) MODE_##name,
enum {
#include "cpumode.list"
};
#undef DEFMODE

/* CPU Mode Functions */

/* Get CSPR */
uint32_t    cur_cpsr(void);
/* Get CPU Mode */
cpumode_t   cur_cpumode(void);
/* Helper functions */
uint32_t    cpumode_bits(cpumode_t);
cpumode_t   cpumode_from_bits(uint32_t);
const char *cpumode_name(cpumode_t);

#endif
