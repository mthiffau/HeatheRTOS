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
#include "hw_cm_per.h"
#include "hw_cm_dpll.h"
#include "hw_types.h"

#include "dmtimer.h"

/*******************************************************************************
                       INTERNAL MACRO DEFINITIONS
*******************************************************************************/
#define CAPT_REG_1                         (1u)
#define CAPT_REG_2                         (2u)

/*******************************************************************************
                        API FUNCTION DEFINITIONS
*******************************************************************************/

/**
 * This API will start the timer. 
 *
 * baseAdd Base Address of the DMTimer Module Register.
 *
 * return  None.
 *
 * The timer must be configured before it is started/enabled.
 *
 **/
void 
DMTimerEnable(unsigned int baseAdd)
{
    /* Start the timer */
    HWREG(baseAdd + DMTIMER_TCLR) |= DMTIMER_TCLR_ST;
}

/**
 * This API will stop the timer. 
 *
 * baseAdd Base Address of the DMTimer Module Register.
 *
 * return  None.
 *
 **/
void 
DMTimerDisable(unsigned int baseAdd)
{
    /* Stop the timer */
    HWREG(baseAdd + DMTIMER_TCLR) &= ~DMTIMER_TCLR_ST;
}

/**
 * This API will configure the timer in combinations of 
 * 'One Shot timer' and 'Compare' Mode or 'Auto-reload timer' 
 * and 'Compare' Mode.  
 *
 * baseAdd   Base Address of the DMTimer Module Register.
 * timerMode Mode for enabling the timer.
 *
 * 'timerMode' can take the following values
 *    DMTIMER_ONESHOT_CMP_ENABLE - One shot and compare mode enabled
 *    DMTIMER_ONESHOT_NOCMP_ENABLE - One shot enabled, compare mode disabled
 *    DMTIMER_AUTORLD_CMP_ENABLE - Auto-reload and compare mode enabled
 *    DMTIMER_AUTORLD_NOCMP_ENABLE - Auto-reload enabled, compare mode
 *                                   disabled
 *
 * return  None.
 *
 **/
void 
DMTimerModeConfigure(unsigned int baseAdd, unsigned int timerMode)
{
    /* Clear the AR and CE field of TCLR */
    HWREG(baseAdd + DMTIMER_TCLR) &= ~(DMTIMER_TCLR_AR | DMTIMER_TCLR_CE);

    /* Set the timer mode in TCLR register */
    HWREG(baseAdd + DMTIMER_TCLR) |= (timerMode & (DMTIMER_TCLR_AR | 
                                                   DMTIMER_TCLR_CE));
}

/**
 * This API will configure and enable the pre-scaler clock.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 * ptv     Pre-scale clock Timer value.
 *
 * 'ptv' can take the following values \n
 *    DMTIMER_PRESCALER_CLK_DIV_BY_2 - Timer clock divide by 2 \n
 *    DMTIMER_PRESCALER_CLK_DIV_BY_4 - Timer clock divide by 4 \n
 *    DMTIMER_PRESCALER_CLK_DIV_BY_8 - Timer clock divide by 8 \n
 *    DMTIMER_PRESCALER_CLK_DIV_BY_16 - Timer clock divide by 16 \n
 *    DMTIMER_PRESCALER_CLK_DIV_BY_32 - Timer clock divide by 32 \n
 *    DMTIMER_PRESCALER_CLK_DIV_BY_64 - Timer clock divide by 64 \n
 *    DMTIMER_PRESCALER_CLK_DIV_BY_128 - Timer clock divide by 128 \n
 *    DMTIMER_PRESCALER_CLK_DIV_BY_256 - Timer clock divide by 256 \n
 *
 * return  None.
 *
 **/
void 
DMTimerPreScalerClkEnable(unsigned int baseAdd, unsigned int ptv)
{
    /* Clear the PTV field of TCLR */
    HWREG(baseAdd + DMTIMER_TCLR) &= ~DMTIMER_TCLR_PTV;

    /* Set the PTV field and enable the pre-scaler clock */
    HWREG(baseAdd + DMTIMER_TCLR) |= (ptv & (DMTIMER_TCLR_PTV | DMTIMER_TCLR_PRE));
}

/**
 * This API will disable the pre-scaler clock.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 *
 * return  None.
 *
 **/
void 
DMTimerPreScalerClkDisable(unsigned int baseAdd)
{
    /* Disable Pre-scaler clock */
    HWREG(baseAdd + DMTIMER_TCLR) &= ~DMTIMER_TCLR_PRE;
}

/**
 * Set/Write the Counter register with the counter value.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 * counter Count value for the timer.
 *
 * return  None.
 *
 * Value can be loaded into the counter register when the counter is 
 * stopped or when it is running.
 *
 **/
void 
DMTimerCounterSet(unsigned int baseAdd, unsigned int counter)
{
    /* Set the counter value */
    HWREG(baseAdd + DMTIMER_TCRR) = counter;
}

/**
 * Get/Read the counter value from the counter register.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 *
 * return  This API returns the count value present in the DMTimer Counter
 *          register.
 *
 * Value can be read from the counter register when the counter is
 * stopped or when it is running.
 *
 **/
unsigned int 
DMTimerCounterGet(unsigned int baseAdd)
{
    /* Read the counter value from TCRR */
    return (HWREG(baseAdd + DMTIMER_TCRR));
}

/**
 * Set the reload count value in the timer load register.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 * reload  The reload count value of the timer.
 *
 * return  None.
 *
 * It is recommended to not use reload value as 0xFFFFFFFF as it can 
 * lead to undesired results.
 *
 **/
void DMTimerReloadSet(unsigned int baseAdd, unsigned int reload)
{
    /* Load the register with the re-load value */
    HWREG(baseAdd + DMTIMER_TLDR) = reload;
}

/**
 * Get the reload count value from the timer load register.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 *
 * return  This API returns the value present in DMTimer Load Register.
 *
 **/
unsigned int 
DMTimerReloadGet(unsigned int baseAdd)
{
    /* Return the contents of TLDR */
    return (HWREG(baseAdd + DMTIMER_TLDR));
}

/**
 * Configure general purpose output pin.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 * gpoCfg  General purpose output.
 * 
 * 'gpoCfg' can take the following values
 *    DMTIMER_GPO_CFG_0 - PORGPOCFG drives 0
 *    DMTIMER_GPO_CFG_1 - PORGPOCFG drives 1
 *
 * return  None.
 *
 **/
void 
DMTimerGPOConfigure(unsigned int baseAdd, unsigned int gpoCfg)
{
    /* Clear the GPO_CFG field of TCLR */
    HWREG(baseAdd + DMTIMER_TCLR) &= ~DMTIMER_TCLR_GPO_CFG;

    /* Write to the gpoCfg field of TCLR */
    HWREG(baseAdd + DMTIMER_TCLR) |= (gpoCfg & DMTIMER_TCLR_GPO_CFG);
}

/**
 * Set the match register with the compare value.
 *
 * baseAdd    Base Address of the DMTimer Module Register.
 * compareVal Compare value.
 *
 * return     None.
 *
 **/
void 
DMTimerCompareSet(unsigned int baseAdd, unsigned int compareVal)
{
    /* Write the compare value to TMAR */
    HWREG(baseAdd + DMTIMER_TMAR) = compareVal;
}

/**
 * Get the match register contents.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 *
 * return  This API returns the match register contents.
 *
 **/
unsigned int 
DMTimerCompareGet(unsigned int baseAdd)
{
    /* Return the TMAR value */
    return (HWREG(baseAdd + DMTIMER_TMAR));
}

/**
 * Trigger IRQ event by software.
 *
 * baseAdd  Base Address of the DMTimer Module Register.
 * intFlags Variable used to trigger the events.
 *
 * 'intFlags' can take the following values \n
 *    DMTIMER_INT_TCAR_IT_FLAG - Trigger IRQ status for capture \n
 *    DMTIMER_INT_OVF_IT_FLAG - Trigger IRQ status for overflow \n
 *    DMTIMER_INT_MAT_IT_FLAG - Trigger IRQ status for match \n
 *
 * return  None.
 *
 **/
void 
DMTimerIntRawStatusSet(unsigned int baseAdd, unsigned int intFlags)
{
    /* Trigger the events in IRQSTATUS_RAW register */
    HWREG(baseAdd + DMTIMER_IRQSTATUS_RAW) = (intFlags & 
                                           (DMTIMER_IRQSTATUS_RAW_MAT_IT_FLAG |
                                            DMTIMER_IRQSTATUS_RAW_OVF_IT_FLAG |
                                            DMTIMER_IRQSTATUS_RAW_TCAR_IT_FLAG));
}

/**
 * Read the status of IRQSTATUS_RAW register.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 *
 * return  This API returns the contents of IRQSTATUS_RAW register.
 *
 **/
unsigned int 
DMTimerIntRawStatusGet(unsigned int baseAdd)
{
    /* Return the status of IRQSTATUS_RAW register */
    return (HWREG(baseAdd + DMTIMER_IRQSTATUS_RAW));
}

/**
 * Read the status of IRQ_STATUS register.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 *
 * return  This API returns the status of IRQSTATUS register.
 *
 **/
unsigned int 
DMTimerIntStatusGet(unsigned int baseAdd)
{
    /* Return the status of IRQSTATUS register */
    return (HWREG(baseAdd + DMTIMER_IRQSTATUS));
}

/**
 * Clear the status of interrupt events.
 *
 * baseAdd  Base Address of the DMTimer Module Register.
 * intFlags Variable used to clear the events.
 *
 * 'intFlags' can take the following values \n
 *    DMTIMER_INT_TCAR_IT_FLAG - Clear IRQ status for capture \n
 *    DMTIMER_INT_OVF_IT_FLAG - Clear IRQ status for overflow \n
 *    DMTIMER_INT_MAT_IT_FLAG - Clear IRQ status for match \n
 *
 * return  None.
 *
 **/
void 
DMTimerIntStatusClear(unsigned int baseAdd, unsigned int intFlags)
{
    /* Clear the interrupt status from IRQSTATUS register */
    HWREG(baseAdd + DMTIMER_IRQSTATUS) = (intFlags & 
                                         (DMTIMER_IRQSTATUS_TCAR_IT_FLAG | 
                                          DMTIMER_IRQSTATUS_OVF_IT_FLAG | 
                                          DMTIMER_IRQSTATUS_MAT_IT_FLAG));
}

/**
 * Enable the DMTimer interrupts.
 *
 * baseAdd  Base Address of the DMTimer Module Register.
 * intFlags Variable used to enable the interrupts.
 *
 * 'intFlags' can take the following values \n
 *    DMTIMER_INT_TCAR_EN_FLAG - IRQ enable for capture \n
 *    DMTIMER_INT_OVF_EN_FLAG - IRQ enable for overflow \n
 *    DMTIMER_INT_MAT_EN_FLAG - IRQ enable for match \n
 *
 * return  None.
 *
 **/
void 
DMTimerIntEnable(unsigned int baseAdd, unsigned int intFlags)
{
    /* Enable the DMTimer interrupts represented by intFlags */
    HWREG(baseAdd + DMTIMER_IRQENABLE_SET) = (intFlags & 
                                           (DMTIMER_IRQENABLE_SET_TCAR_EN_FLAG |
                                            DMTIMER_IRQENABLE_SET_OVF_EN_FLAG | 
                                            DMTIMER_IRQENABLE_SET_MAT_EN_FLAG));
}

/**
 * Disable the DMTimer interrupts.
 *
 * baseAdd  Base Address of the DMTimer Module Register.
 * intFlags Variable used to disable the interrupts.
 *
 * 'intFlags' can take the following values \n
 *    DMTIMER_INT_TCAR_EN_FLAG - IRQ disable for capture \n
 *    DMTIMER_INT_OVF_EN_FLAG - IRQ disable for overflow \n
 *    DMTIMER_INT_MAT_EN_FLAG - IRQ disable for match \n
 *
 * return  None.
 *
 **/
void 
DMTimerIntDisable(unsigned int baseAdd, unsigned int intFlags)
{
    /* Disable the DMTimer interrupts represented by intFlags */
    HWREG(baseAdd + DMTIMER_IRQENABLE_CLR) = (intFlags &
                                           (DMTIMER_IRQENABLE_CLR_TCAR_EN_FLAG |
                                            DMTIMER_IRQENABLE_CLR_OVF_EN_FLAG |
                                            DMTIMER_IRQENABLE_CLR_MAT_EN_FLAG));
}

/**
 * Set/enable the trigger write access.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 *
 * return  None.
 *
 *     When we have enabled the timer in Auto-reload mode, the value from 
 *          TLDR is reloaded into TCRR when a overflow condition occurs. But if 
 *           we want to load the contents from TLDR to TCRR before overflow 
 *          occurs then call this API.
 **/
void 
DMTimerTriggerSet(unsigned int baseAdd)
{
    /* Write a value to the register */
    HWREG(baseAdd + DMTIMER_TTGR) = DMTIMER_TTGR_TTGR_VALUE;
}

/**
 * Read the status of IRQENABLE_SET register.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 *
 * return  This API returns the status of IRQENABLE_SET register.
 *
 **/
unsigned int 
DMTimerIntEnableGet(unsigned int baseAdd)
{
    /* Return the status of register IRQENABLE_SET */
    return (HWREG(baseAdd + DMTIMER_IRQENABLE_SET));
}

/**
 * Enable/Disable software reset for DMTimer module.
 *
 * baseAdd   Base Address of the DMTimer Module Register.
 * rstOption Enable/Disable reset option for DMTimer.
 *          
 * 'rstOption' can take the following values
 *    DMTIMER_SFT_RESET_ENABLE - Software reset is enabled
 *    DMTIMER_SFT_RESET_DISABLE - Software reset is disabled
 *
 * return  None.
 *
 **/
void 
DMTimerResetConfigure(unsigned int baseAdd, unsigned int rstOption)
{
    /* Clear the SFT field of TSICR */
    HWREG(baseAdd + DMTIMER_TSICR) &= ~DMTIMER_TSICR_SFT;

    /* Write the option sent by user to SFT field of TSICR */
    HWREG(baseAdd + DMTIMER_TSICR) |= (rstOption & DMTIMER_TSICR_SFT);
}

/**
 * Reset the DMTimer module.
 *
 * baseAdd Base Address of the DMTimer Module Register.
 *
 * return  None.
 *
 **/
void 
DMTimerReset(unsigned int baseAdd)
{
    /* Reset the DMTimer module */
    HWREG(baseAdd + DMTIMER_TIOCP_CFG) |= DMTIMER_TIOCP_CFG_SOFTRESET;

    while(DMTIMER_TIOCP_CFG_SOFTRESET == (HWREG(baseAdd + DMTIMER_TIOCP_CFG)
                                          & DMTIMER_TIOCP_CFG_SOFTRESET));
}

/**
 * This API stores the context of the DMTimer
 *
 * baseAdd    Base Address of the DMTimer Module Register.
 * contextPtr Pointer to the structure where the DM timer context
 *            need to be saved.
 *
 * return  None.
 *
 **/
void
DMTimerContextSave(unsigned int baseAdd, DMTIMERCONTEXT *contextPtr)
{
    contextPtr->tldr = HWREG(baseAdd + DMTIMER_TLDR);
    contextPtr->tmar = HWREG(baseAdd + DMTIMER_TMAR);
    contextPtr->irqenableset = HWREG(baseAdd + DMTIMER_IRQENABLE_SET);
    contextPtr->tcrr = HWREG(baseAdd + DMTIMER_TCRR);
    contextPtr->tclr = HWREG(baseAdd + DMTIMER_TCLR);
}

/**
 * This API restores the context of the DMTimer
 *
 * baseAdd    Base Address of the DMTimer Module Register.
 * contextPtr Pointer to the structure where the DM timer context
 *            need to be restored from.
 *
 * return  None.
 *
 **/
void 
DMTimerContextRestore(unsigned int baseAdd,  DMTIMERCONTEXT *contextPtr)
{
    HWREG(baseAdd + DMTIMER_TLDR) = contextPtr->tldr;
    HWREG(baseAdd + DMTIMER_TMAR) = contextPtr->tmar;
    HWREG(baseAdd + DMTIMER_IRQENABLE_SET) = contextPtr->irqenableset;
    HWREG(baseAdd + DMTIMER_TCRR) = contextPtr->tcrr;
    HWREG(baseAdd + DMTIMER_TCLR) = contextPtr->tclr;
}
