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

#include "u_tid.h"
#include "clock_srv.h"
#include "blnk_srv.h"

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "pqueue.h"

#include "xassert.h"
#include "u_syscall.h"
#include "ns.h"
#include "array_size.h"

#include "soc_AM335x.h"
#include "hw_types.h"
#include "gpio.h"

#define GPIO_INSTANCE_ADDRESS (SOC_GPIO_1_REGS)
#define GPIO_INSTANCE_PIN_NUMBER (23)

static void blnksrv_init(void);

void
blnksrv_main(void)
{
    int i;
    int wakeup_time = 0;
    int dbl_wu_time = 0;

    blnksrv_init();

    struct clkctx clock;
    clkctx_init(&clock);

    unsigned int pin_state = GPIO_PIN_HIGH;
    unsigned int minute_counter = 0;

    /* Get the initial time */
    wakeup_time = Time(&clock);

    /* Blink Indefinately */
    while(1){

	/* Delay 0.5 Seconds */
	wakeup_time += 500;
	DelayUntil(&clock, wakeup_time);

	/* Flip the LED state */
	pin_state = (pin_state == GPIO_PIN_HIGH ? GPIO_PIN_LOW : GPIO_PIN_HIGH);

	GPIOPinWrite(GPIO_INSTANCE_ADDRESS,
		     GPIO_INSTANCE_PIN_NUMBER,
		     pin_state);

	/* Double blink every minute */
	minute_counter += 1;
	if (minute_counter == 120) {
	    assert(pin_state == GPIO_PIN_HIGH);

	    /* Flip the LED every 250 ms 4 times */
	    dbl_wu_time = wakeup_time;
	    for(i = 0; i < 4; i++) {
		dbl_wu_time += 250;
		DelayUntil(&clock, dbl_wu_time);
		pin_state = (pin_state == GPIO_PIN_HIGH ? GPIO_PIN_LOW : GPIO_PIN_HIGH);
		GPIOPinWrite(GPIO_INSTANCE_ADDRESS,
			     GPIO_INSTANCE_PIN_NUMBER,
			     pin_state);
	    }
	    assert(pin_state == GPIO_PIN_HIGH);
	    assert(dbl_wu_time == wakeup_time + 1000);

	    /* Set things up to go back to half second triggers */
	    wakeup_time = dbl_wu_time;
	    minute_counter = 2;
	}
	
    }
}

static void
blnksrv_init(void)
{
    /* Set up LED pin */
    /* Selecting GPIO1[23] pin for use. */
    GPIO1Pin23PinMuxSetup();
    
    /* Enabling the GPIO module. */
    GPIOModuleEnable(GPIO_INSTANCE_ADDRESS);
    
    /* Resetting the GPIO module. */
    GPIOModuleReset(GPIO_INSTANCE_ADDRESS);
    
    /* Setting the GPIO pin as an output pin. */
    GPIODirModeSet(GPIO_INSTANCE_ADDRESS,
                   GPIO_INSTANCE_PIN_NUMBER,
                   GPIO_DIR_OUTPUT);
}
