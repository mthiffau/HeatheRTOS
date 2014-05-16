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

#include "config.h"
#include "xdef.h"
#include "u_tid.h"
#include "u_init.h"
#include "u_syscall.h"
#include "ns.h"

#include "xbool.h"
#include "xint.h"
#include "poly.h"
#include "static_assert.h"
#include "xassert.h"
#include "cpumode.h"
#include "clock_srv.h"

#include "blnk_srv.h"

#include "xarg.h"
#include "bwio.h"

#include "soc_AM335x.h"
#include "gpio.h"
#include "beaglebone.h"

#include "float_test.h"

#define GPIO_INSTANCE_ADDRESS (SOC_GPIO_1_REGS)
#define GPIO_INSTANCE_PIN_NUMBER (23)

void
u_init_main(void)
{
    tid_t ns_tid, clk_tid, blnk_tid;
    //tid_t float_tid1, float_tid2, float_tid3;
    //tid_t hw_tid;
    //struct serialcfg ttycfg, traincfg;
    //int rplylen;

    /* Start the name server. It's important that startup proceeds so that
     * the TID of the name server can be known at compile time (NS_TID).
     * The priority strikes me as relatively unimportant - NS does some
     * work on startup and then probably never again. */
    ns_tid = Create(PRIORITY_NS, &ns_main);
    assertv(ns_tid, ns_tid == NS_TID);

    /* Start clock server */
    clk_tid = Create(PRIORITY_CLOCK, &clksrv_main);
    assertv(clk_tid, clk_tid >= 0);

    /* Start Blink Server */
    blnk_tid = Create(PRIORITY_BLINK, &blnksrv_main);
    assertv(blnk_tid, blnk_tid >= 0);

    /* Floating Point Test Program */
    /*
    float_tid1 = Create(5, &fpu_test_main);
    assertv(float_tid1, float_tid1 >= 0);

    float_tid2 = Create(5, &fpu_test_main);
    assertv(float_tid2, float_tid2 >= 0);

    float_tid3 = Create(5, &fpu_test_main);
    assertv(float_tid3, float_tid3 >= 0);
    */

    /* Start serial server for TTY */
    /*
    tty_tid = Create(PRIORITY_SERIAL2, &serialsrv_main);
    assertv(tty_tid, tty_tid >= 0);
    ttycfg = (struct serialcfg) {
        .uart   = COM2,
        .fifos  = true,
        .nocts  = true,
        .baud   = 115200,
        .parity = false,
        .parity_even = false,
        .stop2  = false,
        .bits   = 8
    };
    rplylen = Send(tty_tid, &ttycfg, sizeof (ttycfg), NULL, 0);
    assertv(rplylen, rplylen == 0);
    */
}
