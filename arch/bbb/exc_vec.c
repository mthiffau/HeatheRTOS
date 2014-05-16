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
/*
* Some of this code is pulled from TI's BeagleBone Black StarterWare
* Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
*
*  Redistribution and use in source and binary forms, with or without 
*  modification, are permitted provided that the following conditions 
*  are met:
*
*    Redistributions of source code must retain the above copyright 
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the 
*    documentation and/or other materials provided with the   
*    distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

/* Code to set up exception vectors for beaglebone black */

#include "config.h"
#include "xint.h"
#include "xarg.h"
#include "xdef.h"
#include "static_assert.h"
#include "bwio.h"

#include "u_tid.h"
#include "task.h"

#include "cp15.h"
#include "ctx_switch.h"
#include "link.h"
#include "exc_vec.h"

/* This is where we're going to put the exception vector table */
const unsigned int AM335X_VECTOR_BASE = (unsigned int)ExcVecLoc;

/* Place holders for things we don't handle yet... */
static void entry(void);
//static void undef_inst_handler(void);
static void prefetch_abort_handler(void);
static void data_abort_handler(void);
static void FIQ_handler(void);

/* These are the things going into the exception vector table */
static unsigned int const vecTbl[15] = 
{
    0xE59FF018, // Opcode for loading PC with the contents of [PC + 0x18] reset
    0xE59FF018, // Opcode for loading PC with the contents of [PC + 0x18] undef instr
    0xE59FF018, // Opcode for loading PC with the contents of [PC + 0x18] supervisor call
    0xE59FF018, // Opcode for loading PC with the contents of [PC + 0x18] pref abort
    0xE59FF018, // Opcode for loading PC with the contents of [PC + 0x18] data abort
    0xE24FF008, // Opcode for loading PC with (PC - 8) (eq. to while(1))  not used
    0xE59FF014, // Opcode for loading PC with the contents of [PC + 0x10] IRQ
    0xE59FF014, // Opcode for loading PC with the contents of [PC + 0x10] FIQ
    (unsigned int)entry,
    (unsigned int)kern_entry_undef,
    (unsigned int)kern_entry_swi,
    (unsigned int)prefetch_abort_handler,
    (unsigned int)data_abort_handler,
    (unsigned int)kern_entry_irq,
    (unsigned int)FIQ_handler
};

/* Load vector table into memory */
void 
load_vector_table(void)
{
    unsigned int *dest = (unsigned int *)AM335X_VECTOR_BASE;
    unsigned int *src = (unsigned int *)vecTbl;
    unsigned int count;
  
    CP15VectorBaseAddrSet(AM335X_VECTOR_BASE);

    for(count = 0; count < sizeof(vecTbl)/sizeof(vecTbl[0]); count++)
    {
	dest[count] = src[count];
    }
}

/* Print vector table */
void
print_vector_table(void)
{
    unsigned int count;
    unsigned int *entry = (unsigned int*)AM335X_VECTOR_BASE;
    bwprintf("\n\rException Vector\n\r");
    for(count = 0; count < sizeof(vecTbl)/sizeof(vecTbl[0]); count++) {
	bwprintf("%x\n\r", entry[count]);
    }
}

/* This was defined by starterware...not sure what it's supposed to do */
static void
entry(void)
{
    bwputstr("Whatever entry is...this is it.\n\r");
    while(1);
}

/* Handle a prefetch abort */
static void
prefetch_abort_handler(void)
{
    int lr = 0;
    asm volatile("mov %[out],lr" : [out] "=r" (lr) ::);
    bwputstr("Something you did caused a prefetch abort...\n\r");
    bwprintf("Problem instruction at: %x\n\r", lr);
    bwputstr("Go fix it.\n\r");
    while(1);
}

/* Handle a data abort */
static void
data_abort_handler(void)
{
    int lr = 0;
    asm volatile("mov %[out],lr" : [out] "=r" (lr) ::);
    bwputstr("Something you did caused a data abort...\n\r");
    bwprintf("Problem instruction at: %x\n\r", lr);
    bwputstr("Go fix it.\n\r");
    while(1);
}

/* Handle an FIQ */
static void
FIQ_handler(void)
{
    bwputstr("FIQ's are not supported on this system!\n\r");
    while(1);
}
