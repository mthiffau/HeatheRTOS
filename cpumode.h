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
