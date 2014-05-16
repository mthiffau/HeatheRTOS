/*******************************************************************************
    Copyright 2014 Matthew Thiffault

    This file is part of HeatheRTOS.

    HeatheRTOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    HeatheRTOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HeatheRTOS.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************/

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
