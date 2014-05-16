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
#include "timer.h"

#include "xdef.h"
#include "static_assert.h"

#include "soc_AM335x.h"
#include "hw_types.h"

#include "dmtimer.h"

/* Fwd Decl. DMTimer2 Clock src enable/select */
void DMTimer2ModuleClkConfig(void);

/* Configure a timer to use for debug purposes */
void dbg_tmr_setup(void)
{
    /* Call the reset code */
    dbg_tmr_reset();
}

/* Reset the debug timer */
void dbg_tmr_reset(void)
{
    /* Disable timer */
    DMTimerDisable(SOC_DMTIMER_2_REGS);

    /* Load the counter with 0 */
    DMTimerCounterSet(SOC_DMTIMER_2_REGS, 0x0);

    /* Load the load register with the reload count value of 0 */
    DMTimerReloadSet(SOC_DMTIMER_2_REGS, 0x0);

    /* Make timer auto-reload, no compare */
    DMTimerModeConfigure(SOC_DMTIMER_2_REGS, DMTIMER_AUTORLD_NOCMP_ENABLE);

    /* Set pre-scaler to divide by 8 */
    DMTimerPreScalerClkEnable(SOC_DMTIMER_2_REGS, DMTIMER_PRESCALER_CLK_DIV_BY_8);

    /* Start the timer */
    DMTimerEnable(SOC_DMTIMER_2_REGS);
}

/* Get the value of the debug timer (in us) */
uint32_t dbg_tmr_get(void)
{
    return (DMTimerCounterGet(SOC_DMTIMER_2_REGS) / 3);
}
