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

#include "cpu.h"

/*****************************************************************************
                   FUNCTION DEFINITIONS
******************************************************************************/

/*
**
** Wrapper function for the IRQ status
**
*/
unsigned int CPUIntStatus(void)
{
    unsigned int stat;

    /* IRQ and FIQ in CPSR */
    asm("    mrs     r0, CPSR\n\t"
        "    and     %[result], r0, #0xC0" : [result] "=r" (stat));
    
    return stat;
}

/*
**
** Wrapper function for the IRQ disable function
**
*/
void CPUirqd(void)
{
    /* Disable IRQ in CPSR */
    asm("    mrs     r0, CPSR\n\t"
        "    orr     r0, #0x80\n\t"
        "    msr     CPSR, r0");
}

/*
**
** Wrapper function for the IRQ enable function
**
*/
void CPUirqe(void)
{
    /* Enable IRQ in CPSR */
    asm("    mrs     r0, CPSR\n\t"
        "    bic     r0, #0x80\n\t"
        "    msr     CPSR, r0");
}

/*
**
** Wrapper function for the FIQ disable function
**
*/
void CPUfiqd(void)
{
    /* Disable FIQ in CPSR */
    asm("    mrs     r0, CPSR\n\t"
        "    orr     r0, #0x40\n\t"
        "    msr     CPSR, r0");
}

/*
**
** Wrapper function for the FIQ enable function
**
*/
void CPUfiqe(void)
{
    /* Enable FIQ in CPSR */
    asm("    mrs     r0, CPSR\n\t"
        "    bic     r0, #0x40\n\t"
        "    msr     CPSR, r0");
}

