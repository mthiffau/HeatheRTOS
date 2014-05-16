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

#include "xbool.h"
#include "xint.h"
#include "intr.h"

#include "xdef.h"
#include "static_assert.h"
#include "bithack.h"

#include "xarg.h"
#include "bwio.h"

#include "soc_AM335x.h"
#include "hw_intc.h"
#include "interrupt.h"

/* Enable/Disable a particular IRQ */
void
intr_enable(int intr, bool enable)
{
    if(enable) {
	IntSystemEnable(intr);
    } else {
	IntSystemDisable(intr);
    }
}

/* Configure an IRQ priority/FIQ enable */
void
intr_config(int intr, unsigned int prio, bool fiq)
{
    IntPrioritySet(intr, 
		   prio, 
		   fiq ? AINTC_HOSTINT_ROUTE_FIQ : AINTC_HOSTINT_ROUTE_IRQ);
}

/* Return the current highest priority interrupt */
int
intr_cur()
{
    return IntActiveIrqNumGet();
}

/* Reset the interrupt controller */
void
intr_reset()
{
    IntMasterIRQDisable();
    IntMasterIRQEnable();
    IntAINTCInit();
}

/* Re-enable interrupts after trapping */
void
intr_acknowledge(void)
{
    volatile unsigned int* 
	intc_control_reg = (unsigned int*)(SOC_AINTC_REGS + INTC_CONTROL);
    *intc_control_reg = INTC_CONTROL_NEWIRQAGR;

    __asm__ volatile ("dsb":::"memory");
}
