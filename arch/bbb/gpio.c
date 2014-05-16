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

#include "soc_AM335x.h"
#include "hw_types.h"
#include "hw_control_AM335x.h"
#include "pin_mux.h"
#include "gpio.h"

/*****************************************************************************
                       FUNCTION DEFINITIONS                                
*****************************************************************************/

/**
 * This API performs the module reset of the GPIO module. It also
 * waits until the reset process is complete.
 *
 * baseAdd    The memory address of the GPIO instance being used
 *
 * return None
 */

void 
GPIOModuleReset(unsigned int baseAdd)
{
    /*
    ** Setting the SOFTRESET bit in System Configuration register.
    ** Doing so would reset the GPIO module.
    */
    HWREG(baseAdd + GPIO_SYSCONFIG) |= (GPIO_SYSCONFIG_SOFTRESET);
    
    /* Waiting until the GPIO Module is reset.*/
    while(!(HWREG(baseAdd + GPIO_SYSSTATUS) & GPIO_SYSSTATUS_RESETDONE));
}

/**
 * This API is used to enable the GPIO module. When the GPIO module
 * is enabled, the clocks to the module are not gated.
 *
 * baseAdd    The memory address of the GPIO instance being used
 *
 * return None
 *
 * Enabling the GPIO module is a primary step before any other
 * configurations can be done.
 */

void 
GPIOModuleEnable(unsigned int baseAdd)
{
    /* Clearing the DISABLEMODULE bit in the Control(CTRL) register. */
    HWREG(baseAdd + GPIO_CTRL) &= ~(GPIO_CTRL_DISABLEMODULE);
}

/**
 * This API is used to disable the GPIO module. When the GPIO module
 * is disabled, the clocks to the module are gated.
 *
 * baseAdd The memory address of the GPIO instance being used
 *
 * return  None
 */

void 
GPIOModuleDisable(unsigned int baseAdd)
{
    /* Setting the DISABLEMODULE bit in Control(CTRL) register. */
    HWREG(baseAdd + GPIO_CTRL) |= (GPIO_CTRL_DISABLEMODULE);
}

/**
 * This API configures the direction of a specified GPIO pin as being
 * either input or output.
 *
 * baseAdd      The memory address of the GPIO instance being used
 * pinNumber    The number of the pin in the GPIO instance
 * pinDirection The required direction for the GPIO pin
 *
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 *
 * 'pinDirection' can take one of the following values:
 * - GPIO_DIR_INPUT - to configure the pin as an input pin
 * - GPIO_DIR_OUTPUT - to configure the pin as an output pin
 *
 * return None
 */

void 
GPIODirModeSet(unsigned int baseAdd,
	       unsigned int pinNumber,
	       unsigned int pinDirection)
{
    /* Checking if pin is required to be an output pin. */
    if(GPIO_DIR_OUTPUT == pinDirection)
    {
        HWREG(baseAdd + GPIO_OE) &= ~(1 << pinNumber);
    }
    else
    {
        HWREG(baseAdd + GPIO_OE) |= (1 << pinNumber);
    }
}

/**
 * This API determines the direction of a specified GPIO pin.
 * 
 * baseAdd   The memory address of the GPIO instance being used
 * pinNumber The number of the pin in the GPIO instance
 *
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 * 
 * return  The direction of the specified pin. This returns one of the
 *
 * following two values:
 * - GPIO_DIR_INPUT - signifying that the pin is an input pin
 * - GPIO_DIR_OUTPUT - signifying that the pin is an output pin
 *
 */

unsigned int 
GPIODirModeGet(unsigned int baseAdd,
	       unsigned int pinNumber)
{
    unsigned int retVal = GPIO_DIR_INPUT;

    if(!(HWREG(baseAdd + GPIO_OE) & (1 << pinNumber)))
    {
        retVal = GPIO_DIR_OUTPUT;
    }

    return retVal;
}

/**
 * This API drives an output GPIO pin to a logic HIGH or a logic LOW
 * state.
 *
 * baseAdd     The memory address of the GPIO instance being used
 * pinNumber   The number of the pin in the GPIO instance
 * pinValue    This specifies whether a logic HIGH or a logic LOW
 *             should be driven on the output pin
 *
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 *
 * 'pinValue' can take one of the following values:
 * - GPIO_PIN_LOW - indicating to drive a logic LOW(logic 0) on the pin.
 * - GPIO_PIN_HIGH - indicating to drive a logic HIGH(logic 1) on the pin.
 *
 * return None
 *
 */

void 
GPIOPinWrite(unsigned int baseAdd,
	     unsigned int pinNumber,
	     unsigned int pinValue)
{
    if(GPIO_PIN_HIGH == pinValue)
    {
        HWREG(baseAdd + GPIO_SETDATAOUT) = (1 << pinNumber);
    }
    else
    {
        HWREG(baseAdd + GPIO_CLEARDATAOUT) = (1 << pinNumber);
    }
}

/**
 * This API determines the logic level(value) on a specified
 * GPIO pin.
 *
 * baseAdd   The memory address of the GPIO instance being used
 * pinNumber The number of the pin in the GPIO instance
 *
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 *
 * return  The pin value of the specified pin of the GPIO instance.
 */

unsigned int 
GPIOPinRead(unsigned int baseAdd,
	    unsigned int pinNumber)
{
    return(HWREG(baseAdd + GPIO_DATAIN) & (1 << pinNumber));
}

/**
 * This API is used to collectively set and collectively clear the
 * specified output pins of a GPIO instance.
 *
 * baseAdd    The memory address of the GPIO instance being used
 * setMask    The bit mask of the bits which have to be set in the
 *            GPIO Data Output Register(GPIO_DATAOUT)
 * clrMask    The bit mask of the bits which have to cleared in the
 *            GPIO Data Output Register(GPIO_DATAOUT)
 *
 * return None
 */

void 
GPIOMultiplePinsWrite(unsigned int baseAdd,
		      unsigned int setMask,
		      unsigned int clrMask)
{
    /* Setting the specified output pins in GPIO_DATAOUT register. */
    HWREG(baseAdd + GPIO_SETDATAOUT) = setMask;

    /* Clearing the specified output pins in GPIO_DATAOUT register. */
    HWREG(baseAdd + GPIO_CLEARDATAOUT) = clrMask;
}

/**
 * This API reads the pin values of the specified pins of the GPIO
 * instance.
 *
 * baseAdd  The memory address of the GPIO instance being used
 * readMask The bit mask of the bits whose values have to be read
 *          from the Data Input Register(GPIO_DATAIN).
 *
 * return  The pin values of the specified pins of the GPIO instance.
 */

unsigned int 
GPIOMultiplePinsRead(unsigned int baseAdd,
		     unsigned int readMask)
{
    return(HWREG(baseAdd + GPIO_DATAIN) & readMask);
}

/**
 * This API enables the configured interrupt event on a specified input
 * GPIO pin to trigger an interrupt request.
 *
 * baseAdd   The memory address of the GPIO instance being used
 * intLine   This specifies the interrupt request line on which the
 *           interrupt request due to the transitions on a specified
 *           pin be propagated
 * pinNumber The number of the pin in the GPIO instance
 *
 * 'intLine' can take one of the following two values:
 * - GPIO_INT_LINE_1 - interrupt request be propagated over interrupt line 1
 * - GPIO_INT_LINE_2 - interrupt request be propagated over interrupt line 2
 * 
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 *
 * return None
 */

void 
GPIOPinIntEnable(unsigned int baseAdd,
		 unsigned int intLine,
		 unsigned int pinNumber)
{
    if(GPIO_INT_LINE_1 == intLine)
    {
        HWREG(baseAdd + GPIO_IRQSTATUS_SET(0)) = (1 << pinNumber);
    }
    else     
    {
        HWREG(baseAdd + GPIO_IRQSTATUS_SET(1)) = (1 << pinNumber);
    }
}

/**
 * This API disables interrupt generation due to the detection
 * of any event on a specified input GPIO pin.
 *
 * baseAdd    The memory address of the GPIO instance being used
 * intLine    This specifies the interrupt requeset line which has to
 *            disabled to transmit interrupt requests due to transitions
 *            on a specified pin
 * pinNumber  The number of the pin in the GPIO instance
 *
 * 'intLine' can take one of the following two values:
 * - GPIO_INT_LINE_1 - signifying that the Interrupt Line 1 be disabled to
 *   transmit interrupt requests due to transitions on specified pin
 * - GPIO_INT_LINE_2 - signifying that the Interrupt Line 2 be disabled to
 *   transmit interrupt requests due to transitions on specified pin
 *
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 *
 * return  None
 */

void 
GPIOPinIntDisable(unsigned int baseAdd,
		  unsigned int intLine,
		  unsigned int pinNumber)
{
    if(GPIO_INT_LINE_1 == intLine)
    {
        HWREG(baseAdd + GPIO_IRQSTATUS_CLR(0)) = (1 << pinNumber);
    }
    else 
    {
        HWREG(baseAdd + GPIO_IRQSTATUS_CLR(1)) = (1 << pinNumber);
    }
}

/**
 * This API configures the event type for a specified input GPIO pin.
 * Whenever the selected event occurs on that GPIO pin and if interrupt
 * generation is enabled for that pin, the GPIO module will send an
 * interrupt to CPU.
 *
 * baseAdd    The memory address of the GPIO instance being used
 * pinNumber  The number of the pin in the GPIO instance
 * eventType  This specifies the event type on whose detection,
 *            the GPIO module will send an interrupt to CPU,
 *            provided interrupt generation for that pin is enabled.
 *
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 *
 * 'eventType' can take one of the following values:
 * - GPIO_INT_TYPE_NO_LEVEL - no interrupt request on occurence of either a
 *   logic LOW or a logic HIGH on the input GPIO pin
 * - GPIO_INT_TYPE_LEVEL_LOW - interrupt request on occurence of a LOW level
 *   (logic 0) on the input GPIO pin
 * - GPIO_INT_TYPE_LEVEL_HIGH - interrupt request on occurence of a HIGH level
 *   (logic 1) on the input GPIO pin
 * - GPIO_INT_TYPE_BOTH_LEVEL - interrupt request on the occurence of both the
 *   LOW level and HIGH level on the input GPIO pin
 * - GPIO_INT_TYPE_NO_EDGE -  no interrupt request on either rising or
 *   falling edges on the pin
 * - GPIO_INT_TYPE_RISE_EDGE - interrupt request on occurence of a rising edge
 *   on the input GPIO pin
 * - GPIO_INT_TYPE_FALL_EDGE - interrupt request on occurence of a falling edge
 *   on the input GPIO pin
 * - GPIO_INT_TYPE_BOTH_EDGE - interrupt request on occurence of both a rising
 *   and a falling edge on the pin
 *
 * return  None
 *
 * A typical use case of this API is explained below:
 * 
 *      If it is initially required that interrupt should be generated on a
 *      LOW level only, then this API can be called with
 *      'GPIO_INT_TYPE_LEVEL_LOW' as the parameter.
 *      At a later point of time, if logic HIGH level only should be made as
 *      the interrupt generating event, then this API has to be first called
 *      with 'GPIO_INT_TYPE_NO_LEVEL' as the parameter and later with
 *      'GPIO_INT_TYPE_LEVEL_HIGH' as the parameter. Doing this ensures that
 *      logic LOW level trigger for interrupts is disabled.
 */

void 
GPIOIntTypeSet(unsigned int baseAdd,
	       unsigned int pinNumber,
	       unsigned int eventType)
{
    eventType &= 0xFF;

    switch(eventType)
    {

    case GPIO_INT_TYPE_NO_LEVEL:

	/* Disabling logic LOW level detect interrupt generation. */
	HWREG(baseAdd + GPIO_LEVELDETECT(0)) &= ~(1 << pinNumber);

	/* Disabling logic HIGH level detect interrupt generation. */
	HWREG(baseAdd + GPIO_LEVELDETECT(1)) &= ~(1 << pinNumber);

        break;

    case GPIO_INT_TYPE_LEVEL_LOW:

	/* Enabling logic LOW level detect interrupt geenration. */
	HWREG(baseAdd + GPIO_LEVELDETECT(0)) |= (1 << pinNumber);

        break;

    case GPIO_INT_TYPE_LEVEL_HIGH:

	/* Enabling logic HIGH level detect interrupt generation. */
	HWREG(baseAdd + GPIO_LEVELDETECT(1)) |= (1 << pinNumber);
        
        break;

    case GPIO_INT_TYPE_BOTH_LEVEL:
            
	/* Enabling logic LOW level detect interrupt geenration. */
	HWREG(baseAdd + GPIO_LEVELDETECT(0)) |= (1 << pinNumber);

	/* Enabling logic HIGH level detect interrupt generation. */
	HWREG(baseAdd + GPIO_LEVELDETECT(1)) |= (1 << pinNumber);
            
        break;

    case GPIO_INT_TYPE_NO_EDGE:
            
	/* Disabling rising edge detect interrupt generation. */
	HWREG(baseAdd + GPIO_RISINGDETECT) &= ~(1 << pinNumber);

	/* Disabling falling edge detect interrupt generation. */
	HWREG(baseAdd + GPIO_FALLINGDETECT) &= ~(1 << pinNumber);

        break;

    case GPIO_INT_TYPE_RISE_EDGE:

	/* Enabling rising edge detect interrupt generation. */
	HWREG(baseAdd + GPIO_RISINGDETECT) |= (1 << pinNumber);

        break;

    case GPIO_INT_TYPE_FALL_EDGE:

	/* Enabling falling edge detect interrupt generation. */
	HWREG(baseAdd + GPIO_FALLINGDETECT) |= (1 << pinNumber);

        break;

    case GPIO_INT_TYPE_BOTH_EDGE:

	/* Enabling rising edge detect interrupt generation. */
	HWREG(baseAdd + GPIO_RISINGDETECT) |= (1 << pinNumber);

	/* Enabling falling edge detect interrupt generation. */
	HWREG(baseAdd + GPIO_FALLINGDETECT) |= (1 << pinNumber);

        break;

    default:
        break;
    }
}

/**
 * This API determines the programmed interrupt event type for a
 * specified input GPIO pin.
 *
 * baseAdd    The memory address of the GPIO instance being used
 * pinNumber  The number of the pin in the GPIO instance
 *
 * return This returns one or a combination of the following values:
 *
 * - GPIO_INT_TYPE_NO_LEVEL - level trigger for interrupt request is disabled
 * - GPIO_INT_TYPE_LEVEL_LOW - logic LOW level event for interrupt request is
 *   enabled
 * - GPIO_INT_TYPE_LEVEL_HIGH - logic HIGH level event for interrupt request is
 *   enabled
 * - GPIO_INT_TYPE_BOTH_LEVEL - both logic LOW and logic HIGH level events
 *   for interrupt request are enabled
 * - GPIO_INT_TYPE_NO_EDGE - edge trigger for interrupt request is disabled
 * - GPIO_INT_TYPE_RISE_EDGE - rising edge event for interrupt request is
 *   enabled
 * - GPIO_INT_TYPE_FALL_EDGE - falling edge event for interrupt request
 *   is enabled
 * - GPIO_INT_TYPE_BOTH_EDGE - both rising and falling edge events for
 *   interrupt request are enabled
 */

unsigned int 
GPIOIntTypeGet(unsigned int baseAdd,
	       unsigned int pinNumber)
{
    unsigned int intEvent = (GPIO_INT_TYPE_NO_LEVEL | GPIO_INT_TYPE_NO_EDGE);

    /* Checking if logic LOW level trigger interrupt is enabled. */ 
    if(HWREG(baseAdd + GPIO_LEVELDETECT(0)) & (1 << pinNumber))
    {
        intEvent &= ~(GPIO_INT_TYPE_NO_LEVEL);
        intEvent |= GPIO_INT_TYPE_LEVEL_LOW;
    }

    /* Checking if logic HIGH level trigger interrupt is enabled. */ 
    if(HWREG(baseAdd + GPIO_LEVELDETECT(1)) & (1 << pinNumber))
    {
        intEvent &= ~(GPIO_INT_TYPE_NO_LEVEL);
        intEvent |= GPIO_INT_TYPE_LEVEL_HIGH;
    }

    /* Checking if rising edge trigger interrupt is enabled. */
    if(HWREG(baseAdd + GPIO_RISINGDETECT) & (1 << pinNumber))
    {
        intEvent &= ~(GPIO_INT_TYPE_NO_EDGE);
        intEvent |= GPIO_INT_TYPE_RISE_EDGE;
    }

    /* Checking if falling edge trigger interrupt is enabled. */
    if(HWREG(baseAdd + GPIO_FALLINGDETECT) & (1 << pinNumber))
    {
        intEvent &= ~(GPIO_INT_TYPE_NO_EDGE);
        intEvent |= GPIO_INT_TYPE_FALL_EDGE;
    }

    return intEvent;
}

/**
 * This API determines the enabled interrupt status of a specified pin.
 *
 * baseAdd    The memory address of the GPIO instance being used
 * intLine    This specifies the interrupt line whose corresponding
 *                    enabled interrupt status register has to be accessed.
 *                    The status of the specified pin is returned in this API.
 * pinNumber  The number of the pin in the GPIO instance
 * 
 * 'intLine' can take one of the following two values:
 * - GPIO_INT_LINE_1 - to read the enabled interrupt status register
 *   corresponding to interrupt line 1
 * - GPIO_INT_LINE_2 - to read the enabled interrupt status register
 *   corresponding to interrupt line 2
 *
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 *
 * return The enabled interrupt status of the pin on the specified interrupt
 *         line. This could either be a non-zero or a zero value.
 *
 */

unsigned int 
GPIOPinIntStatus(unsigned int baseAdd,
		 unsigned int intLine,
		 unsigned int pinNumber)
{
    unsigned int intStatus = 0;

    if(GPIO_INT_LINE_1 == intLine)
    {
        intStatus = (HWREG(baseAdd + GPIO_IRQSTATUS(0)) & (1 << pinNumber));
    }
    else 
    {
        intStatus = (HWREG(baseAdd + GPIO_IRQSTATUS(1)) & (1 << pinNumber));
    }

    return intStatus;
}

/**
 * This API clears the enabled interrupt status of a specified GPIO
 * pin.
 *
 * baseAdd    The memory address of the GPIO instance being used
 * intLine    This specifies the interrupt line whose corresponding
 *                    enabled interrupt status register has to be accessed.
 *                    The status of the specified bit in it is cleared in this
 *                    API.
 * pinNumber  The number of the pin in the GPIO instance
 *
 * 'intLine' can take one of the following two values:
 * - GPIO_INT_LINE_1 - to access the enabled interrupt status register
 *   corresponding to interrupt line 1
 * - GPIO_INT_LINE_2 - to access the enabled interrupt status register
 *   corresponding to interrupt line 2
 * 
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 *
 * return None
 */

void 
GPIOPinIntClear(unsigned int baseAdd,
		unsigned int intLine,
		unsigned int pinNumber)
{
    if(GPIO_INT_LINE_1 == intLine)
    {
        HWREG(baseAdd + GPIO_IRQSTATUS(0)) = (1 << pinNumber);
    }
    else
    {
        HWREG(baseAdd + GPIO_IRQSTATUS(1)) = (1 << pinNumber);
    }
}

/**
 * This API determines the raw interrupt status of the specified GPIO
 * pins in the instance corresponding to the specified interrupt line.
 *
 * baseAdd    The memory address of the GPIO instance being used
 * intLine    This specifies the interrupt line whose corresponding
 *            raw interrupt status register has to be read.
 * readMask   The bit mask of the bits whose values have to be read
 *            from the Raw Interrupt Status Register
 *            (GPIO_IRQSTATUS_RAW).
 *
 * 'intLine' can take one of the following two values:
 * - GPIO_INT_LINE_1 - to read the raw interrupt status register corresponding
 *   to interrupt line 1
 * - GPIO_INT_LINE_2 - to read the raw interrupt status register corresponding
 *   to interrupt line 2
 *
 * return  The raw interrupt status of the specified pins of the GPIO instance
 *          corresponding to a specified interrupt line.
 */

unsigned int 
GPIORawIntStatus(unsigned int baseAdd,
		 unsigned int intLine,
		 unsigned int readMask)
{
    unsigned int intStatus = 0;

    if(GPIO_INT_LINE_1 == intLine)
    {
        intStatus = HWREG(baseAdd + GPIO_IRQSTATUS_RAW(0)) & readMask;
    }
    else  
    {
        intStatus = HWREG(baseAdd + GPIO_IRQSTATUS_RAW(1)) & readMask;
    }

    return intStatus;
}

/**
 * This API manually triggers an interrupt request due to a specified 
 * GPIO pin. 
 *
 * baseAdd    The memory address of the GPIO instance being used
 * intLine    This specifies the interrupt line over which the
 *                    manually triggered interrupt request has to be
 *                    propogated
 * pinNumber  The number of the pin in the GPIO instance.
 *
 * 'intLine' can take one of the following values:
 * - GPIO_INT_LINE_1 - to propogate the interrupt request over interrupt
 *   line 1
 * - GPIO_INT_LINE_2 - to propogate the interrupt request over interrupt
 *   line 2
 *
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 *
 * return None
 *
 * Here an interrupt request is manually triggered by writing to a
 *      specified bit in the Raw Interrupt Status register. As a
 *      pre-requisite, the interrupt generation should be enabled for the
 *      GPIO pin.
 */

void 
GPIOTriggerPinInt(unsigned int baseAdd,
		  unsigned int intLine,
		  unsigned int pinNumber)
{
    if(GPIO_INT_LINE_1 == intLine)
    {
        HWREG(baseAdd + GPIO_IRQSTATUS_RAW(0)) = (1 << pinNumber);
    }
    else
    {
        HWREG(baseAdd + GPIO_IRQSTATUS_RAW(1)) = (1 << pinNumber);
    }
}

/**
 * This API enables the GPIO Wakeup Generation feature for all
 * the pins of the GPIO module.
 *
 * baseAdd    The memory address of the GPIO instance being used.
 *
 * return None
 *
 * 1> A Wakeup signal is generated by an Input GPIO pin when an
 *    expected transition happens on that GPIO pin.
 * 2> For any Input pin of the GPIO module to generate Wakeup
 *    signal, the respective global feature for the module has
 *    to be enabled by invoking this API.
 */

void 
GPIOWakeupGlobalEnable(unsigned int baseAdd)
{
    /* Setting the ENAWAKEUP bit in SYSCONFIG register. */
    HWREG(baseAdd + GPIO_SYSCONFIG) |= (GPIO_SYSCONFIG_ENAWAKEUP);
}

/**
 * This API disables the GPIO Wakeup generation feature for all
 * the pins of the GPIO module.
 *
 * baseAdd    The memory address of the GPIO instance being used.
 *
 * return None
 */

void 
GPIOWakeupGlobalDisable(unsigned int baseAdd)
{
    /* Clearing the ENAWAKEUP bit in SYSCONFIG register. */
    HWREG(baseAdd + GPIO_SYSCONFIG) &= ~(GPIO_SYSCONFIG_ENAWAKEUP);
}

/**
 * This API enables the feature for the specified Input GPIO Pin
 * (Interrupt Request Source) to generate an asynchronous wakeup
 * signal to the CPU.
 *
 * baseAdd     The memory address of the GPIO instance being used.
 * swakeupLine This specifies the Smart Wakeup Interrupt Line over
 *                     which the asynchrounous wakeup signal has to be
 *                     propogated due to the specified Input GPIO pin.
 * pinNumber   The number of the pin in the GPIO instance.
 *
 * 'swakeupLine' can take one of the following values:
 * - GPIO_INT_SWAKEUP_LINE_1 - to propogate the wakeup request over Smart
 *   Wakeup Interrupt Line 1
 * - GPIO_INT_SWAKEUP_LINE_2 - to propagate the wakeup request over Smart
 *   Wakeup Interrupt Line 2
 *
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 *
 * return None
 *
 * 1> Make sure that the GPIO instance in context is allowed to generate
 *    Wakeup interrupts to the CPU. If allowed, then enable Wakeup
 *    interrupt generation feature for that GPIO instance in the Interrupt
 *    Controller.
 * 2> Usually an Input GPIO Pin acts as an Interrupt Request(IRQ) Source.
 *    An expected transition on an Input GPIO Pin can generate a Wakeup
 *   request.
 */

void
GPIOPinIntWakeUpEnable(unsigned int baseAdd,
		       unsigned int swakeupLine,
		       unsigned int pinNumber)
{
    if(GPIO_INT_LINE_1 == swakeupLine)
    {
        HWREG(baseAdd + GPIO_IRQWAKEN(0)) |= (1 << pinNumber);
    }
    else
    {
        HWREG(baseAdd + GPIO_IRQWAKEN(1)) |= (1 << pinNumber);
    }
}

/**
 * This API disables the feature for the specified Input GPIO Pin
 * (Interrupt Request Source) to generate an asynchronous wakeup
 * signal to the CPU.
 *
 * baseAdd     The memory address of the GPIO instance being used.
 * swakeupLine This specifies the Smart Wakeup Interupt Line which has
 *                     to be disabled to propogate any asynchrounous wakeup
 *                     signal due to the specified input GPIO Pin.
 * pinNumber   The number of the pin in the GPIO instance.
 *
 * 'swakeupLine' can take one of the following values:
 * - GPIO_INT_SWAKEUP_LINE_1 - to propogate the wakeup request over Smart
 *   Wakeup Interrupt Line 1
 * - GPIO_INT_SWAKEUP_LINE_2 - to propagate the wakeup request over Smart
 *   Wakeup Interrupt Line 2
 *
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 *
 * return None
 */

void 
GPIOPinIntWakeUpDisable(unsigned int baseAdd,
			unsigned int swakeupLine,
			unsigned int pinNumber)
{
    if(GPIO_INT_LINE_1 == swakeupLine)
    {
        HWREG(baseAdd + GPIO_IRQWAKEN(0)) &= ~(1 << pinNumber);
    }
    else
    {
        HWREG(baseAdd + GPIO_IRQWAKEN(1)) &= ~(1 << pinNumber);
    }
}

/**
 * This API is used to control(enable/disable) the Auto-Idle mode
 * for GPIO.
 *
 * baseAdd    The memory address of the GPIO instance being used
 * modeFlag   This specifies whether to enable or disable the
 *                    Auto-Idle mode for the GPIO instance
 *
 * 'modeFlag' can take one of the following two values:
 * - GPIO_AUTO_IDLE_MODE_ENABLE - to enable the Auto-Idle mode
 * - GPIO_AUTO_IDLE_MODE_DISABLE - to disable the Auto-Idle mode
 *
 * return  None
 */

void 
GPIOAutoIdleModeControl(unsigned int baseAdd, unsigned int modeFlag)
{
    /* Clearing the AUTOIDLE bit in System Configuration Register. */
    HWREG(baseAdd + GPIO_SYSCONFIG) &= ~(GPIO_SYSCONFIG_AUTOIDLE);

    /* Programming the AUTOIDLE bit in System Configuration Register. */
    HWREG(baseAdd + GPIO_SYSCONFIG) |= (modeFlag & GPIO_SYSCONFIG_AUTOIDLE);
}

/**
 * This API is used to configure the Power Management
 * Request/Acknowledgement process for GPIO.
 *
 * baseAdd    The memory address of the GPIO instance being used
 * modeFlag   This specifies the Power Management Request/Acknowledge
 *            mode(Idle Mode Request and Response) to be enabled for
 *            the GPIO instance
 *
 * 'modeFlag' can take one of the following values:
 * - GPIO_IDLE_MODE_FORCE_IDLE - to enable Force-Idle mode
 * - GPIO_IDLE_MODE_NO_IDLE - to enable No-Idle mode
 * - GPIO_IDLE_MODE_SMART_IDLE - to enable Smart-Idle mode
 * - GPIO_IDLE_MODE_SMART_IDLE_WAKEUP - to enable Smart-Idle-Wakeup mode
 *
 * return None
 */

void 
GPIOIdleModeConfigure(unsigned int baseAdd, unsigned int modeFlag)
{
    /* Clearing the IDLEMODE field in System Configuration Register. */
    HWREG(baseAdd + GPIO_SYSCONFIG) &= ~(GPIO_SYSCONFIG_IDLEMODE);

    /* Programming the IDLEMODE field in System Configuration Register. */
    HWREG(baseAdd + GPIO_SYSCONFIG) |= (modeFlag & GPIO_SYSCONFIG_IDLEMODE);
}


/**
 * This API configures the clock gating ratio for the event detection
 * logic.
 *
 * baseAdd    The memory address of the GPIO instance being used
 * configFlag This specifies the clock gating ratio value to be
 *            programmed
 *
 * 'configFlag' can take one of the following values:
 *
 * - GPIO_GR_FUNC_CLK_INTER_CLK_BY_1 - to program the functional clock as
 * interface clock frequeny divided by 1
 * - GPIO_GR_FUNC_CLK_INTER_CLK_BY_2 - to program the functional clock as
 * interface clock frequency divided by 2
 * - GPIO_GR_FUNC_CLK_INTER_CLK_BY_4 - to program the functional clock as
 * interface clock frequency divided by 4
 * - GPIO_GR_FUNC_CLK_INTER_CLK_BY_8 - to program the functional clock as
 * interface clock frequency divided by 8
 *
 * return None
 */

void 
GPIOGatingRatioConfigure(unsigned int baseAdd, unsigned int configFlag)
{
    /* Clearing the GATINGRATIO field in GPIO_CTRL register. */
    HWREG(baseAdd + GPIO_CTRL) &= ~(GPIO_CTRL_GATINGRATIO);

    /* Programming the GATINGRATIO field in GPIO_CTRL register. */
    HWREG(baseAdd + GPIO_CTRL) |= (configFlag & GPIO_CTRL_GATINGRATIO);    
}

/**
 * This API enables/disables debouncing feature for a specified input
 * GPIO pin.
 * 
 * baseAdd      The memory address of the GPIO instance being used
 * pinNumber    The number of the pin in the GPIO instance
 * controlFlag  This specifies whether to enable/disable Debouncing
 *              feature for the specified input pin
 * 
 * 'pinNumber' can take one of the following values:
 * (0 <= pinNumber <= 31)
 *
 * 'controlFlag' can take one of the following values:
 * - GPIO_DEBOUNCE_FUNC_ENABLE - to enable the debouncing feature for the
 *   specified input GPIO pin
 * - GPIO_DEBOUNCE_FUNC_DISABLE - to disable the debouncing feature for the
 *   specified input GPIO pin
 *
 * return None
 */

void 
GPIODebounceFuncControl(unsigned int baseAdd,
			unsigned int pinNumber,
			unsigned int controlFlag)
{
    
    /* Clearing the DEBOUNCEENABLE[n] bit in GPIO_DEBOUNCEENABLE register. */
    HWREG(baseAdd + GPIO_DEBOUNCENABLE) &= ~(1 << pinNumber);

    /* Programming the DEBOUNCEENABLE[n] bit in GPIO_DEBOUNCEENABLE register.*/
    HWREG(baseAdd + GPIO_DEBOUNCENABLE) |= (controlFlag  << pinNumber);
}

/**
 * This API configures the debouncing time for all the input pins of
 * a GPIO instance.
 *
 * baseAdd       The memory address of the GPIO instance being used
 * debounceTime  This specifies the number of debouncing clock pulses
 *               each of 31 microseconds long to be used for debouncing
 *               time. The formula for debouncing time is:
 *               debounce time = ((debounceTime + 1) * 31) microseconds
 *
 * 'debounceTime' can take a value as per below limits:
 * - (0x00 <= debounceTime <= 0xFF)
 *
 * return None
 */

void 
GPIODebounceTimeConfig(unsigned int baseAdd,
		       unsigned int debounceTime)
{
    /* Programming the DEBOUNCETIME field in GPIO_DEBOUNCINGTIME register. */
    HWREG(baseAdd + GPIO_DEBOUNCINGTIME) = (debounceTime & 
                                            GPIO_DEBOUNCINGTIME_DEBOUNCETIME);
}

/**
 * This API reads the contents in the Revision register of the GPIO
 * module.
 *
 * baseAdd    The memory address of the GPIO instance being used
 *
 * return The contents of the GPIO_REVISION register.
 */

unsigned int 
GPIORevisionInfoGet(unsigned int baseAdd)
{
    return(HWREG(baseAdd + GPIO_REVISION));
}


/**
 * This API stores the context of the gpio
 *
 * baseAdd       Base Address of the GPIO Module Register.
 * contextPtr    Pointer to the structure where the GPIO context
 *               need to be saved.
 *
 * return  None
 *
 **/
void 
gpioContextSave(unsigned int baseAdd, GPIOCONTEXT *contextPtr)
{
    contextPtr->ctrl = HWREG(baseAdd + GPIO_CTRL);
    contextPtr->dir = HWREG(baseAdd + GPIO_OE);
}

/**
 * This API restores the context of the GPIO
 *
 * baseAdd       Base Address of the GPIO Module Register.
 * contextPtr    Pointer to the structure where the GPIO context
 *               need to be restored from.
 *
 * return  None.
 *
 **/
void 
gpioContextRestore(unsigned int baseAdd, GPIOCONTEXT *contextPtr)
{
    HWREG(baseAdd + GPIO_CTRL) = contextPtr->ctrl;
    HWREG(baseAdd + GPIO_OE) = contextPtr->dir;
}

/* Do pin multiplexing and select GPIO1[23] for use. */
void 
GPIO1Pin23PinMuxSetup(void)
{
    HWREG(SOC_CONTROL_REGS + CONTROL_CONF_GPMC_A(7)) = CONTROL_CONF_MUXMODE(7);
}

/* Do pin multiplexing for GPIO1 pins */
void 
GPIO1PinMuxSetup(unsigned int pinNo)
{
    HWREG(SOC_CONTROL_REGS + CONTROL_CONF_GPMC_AD(pinNo)) =
	(CONTROL_CONF_GPMC_AD_CONF_GPMC_AD_SLEWCTRL | 	/* Slew rate slow */
	 CONTROL_CONF_GPMC_AD_CONF_GPMC_AD_RXACTIVE |	/* Receiver enabled */
	 (CONTROL_CONF_GPMC_AD_CONF_GPMC_AD_PUDEN & (~CONTROL_CONF_GPMC_AD_CONF_GPMC_AD_PUDEN)) | /* PU_PD enabled */
	 (CONTROL_CONF_GPMC_AD_CONF_GPMC_AD_PUTYPESEL & (~CONTROL_CONF_GPMC_AD_CONF_GPMC_AD_PUTYPESEL)) | /* PD */
	 (CONTROL_CONF_MUXMODE(7))	/* Select mode 7 */
	    );
}

void 
GpioPinMuxSetup(unsigned int offsetAddr, unsigned int padConfValue)
{
    HWREG(SOC_CONTROL_REGS + offsetAddr) = (padConfValue);
}
