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
#include "hw_intc.h"
#include "interrupt.h"
#include "hw_types.h"
#include "soc_AM335x.h"
#include "cpu.h"

/******************************************************************************
                INTERNAL MACRO DEFINITIONS
******************************************************************************/
#define REG_IDX_SHIFT                  (0x05)
#define REG_BIT_MASK                   (0x1F)
#define NUM_INTERRUPTS                 (128u)

/******************************************************************************
                     API FUNCTION DEFINITIONS
******************************************************************************/

/**
 * This API is used to initialize the interrupt controller. This API  
 * shall be called before using the interrupt controller. 
 *
 * param   None
 * 
 * return  None.
 *
 **/
void IntAINTCInit(void)
{
    /* Reset the ARM interrupt controller */
    HWREG(SOC_AINTC_REGS + INTC_SYSCONFIG) = INTC_SYSCONFIG_SOFTRESET;
 
    /* Wait for the reset to complete */
    while((HWREG(SOC_AINTC_REGS + INTC_SYSSTATUS) 
          & INTC_SYSSTATUS_RESETDONE) != INTC_SYSSTATUS_RESETDONE); 
    
    /* Enable any interrupt generation by setting priority threshold */ 
    HWREG(SOC_AINTC_REGS + INTC_THRESHOLD) = 
                                       INTC_THRESHOLD_PRIORITYTHRESHOLD;
}

/**
 * This API assigns a priority to an interrupt and routes it to
 * either IRQ or to FIQ. Priority 0 is the highest priority level
 * Among the host interrupts, FIQ has more priority than IRQ.
 *
 * param   intrNum  - Interrupt number
 * param   priority - Interrupt priority level
 * param   hostIntRoute - The host interrupt IRQ/FIQ to which the interrupt
 *                         is to be routed.
 *     'priority' can take any value from 0 to 127, 0 being the highest and
 *     127 being the lowest priority.              
 *
 *     'hostIntRoute' can take one of the following values
 *             AINTC_HOSTINT_ROUTE_IRQ - To route the interrupt to IRQ
 *             AINTC_HOSTINT_ROUTE_FIQ - To route the interrupt to FIQ
 *
 * return  None.
 *
 **/
void IntPrioritySet(unsigned int intrNum, unsigned int priority,
                    unsigned int hostIntRoute)
{
    HWREG(SOC_AINTC_REGS + INTC_ILR(intrNum)) =
                                 ((priority << INTC_ILR_PRIORITY_SHIFT)
                                   & INTC_ILR_PRIORITY)
                                 | hostIntRoute ;
}

/**
 * This API enables the system interrupt in AINTC. However, for 
 * the interrupt generation, make sure that the interrupt is 
 * enabled at the peripheral level also. 
 *
 * param   intrNum  - Interrupt number
 *
 * return  None.
 *
 **/
void IntSystemEnable(unsigned int intrNum)
{
    /* Disable the system interrupt in the corresponding MIR_CLEAR register */
    HWREG(SOC_AINTC_REGS + INTC_MIR_CLEAR(intrNum >> REG_IDX_SHIFT))
                                   = (0x01 << (intrNum & REG_BIT_MASK));
}

/**
 * This API disables the system interrupt in AINTC. 
 *
 * param   intrNum  - Interrupt number
 *
 * return  None.
 *
 **/
void IntSystemDisable(unsigned int intrNum)
{
    /* Enable the system interrupt in the corresponding MIR_SET register */
    HWREG(SOC_AINTC_REGS + INTC_MIR_SET(intrNum >> REG_IDX_SHIFT)) 
                                   = (0x01 << (intrNum & REG_BIT_MASK));
}

/**
 * Sets the interface clock to be free running
 *
 * param   None.
 *
 * return  None.
 *
 **/
void IntIfClkFreeRunSet(void)
{
    HWREG(SOC_AINTC_REGS + INTC_SYSCONFIG)&= ~INTC_SYSCONFIG_AUTOIDLE; 
}

/**
 * When this API is called,  automatic clock gating strategy is applied
 * based on the interface bus activity. 
 *
 * param   None.
 *
 * return  None.
 *
 **/
void IntIfClkAutoGateSet(void)
{
    HWREG(SOC_AINTC_REGS + INTC_SYSCONFIG)|= INTC_SYSCONFIG_AUTOIDLE; 
}

/**
 * Reads the active IRQ number.
 *
 * param   None
 *
 * return  Active IRQ number.
 *
 **/
unsigned int IntActiveIrqNumGet(void)
{
    return (HWREG(SOC_AINTC_REGS + INTC_SIR_IRQ) &  INTC_SIR_IRQ_ACTIVEIRQ);
}

/**
 * Reads the spurious IRQ Flag. Spurious IRQ flag is reflected in both
 * SIR_IRQ and IRQ_PRIORITY registers of the interrupt controller.
 *
 * param   None
 *
 * return  Spurious IRQ Flag.
 *
 **/
unsigned int IntSpurIrqFlagGet(void)
{
    return ((HWREG(SOC_AINTC_REGS + INTC_SIR_IRQ) 
             & INTC_SIR_IRQ_SPURIOUSIRQ) 
            >> INTC_SIR_IRQ_SPURIOUSIRQ_SHIFT);
}

/**
 * Enables protection mode for the interrupt controller register access.
 * When the protection is enabled, the registers will be accessible only
 * in privileged mode of the CPU.
 *
 * param   None
 *
 * return  None
 *
 **/
void IntProtectionEnable(void)
{
    HWREG(SOC_AINTC_REGS + INTC_PROTECTION) = INTC_PROTECTION_PROTECTION;
}

/**
 * Disables protection mode for the interrupt controller register access.
 * When the protection is disabled, the registers will be accessible 
 * in both unprivileged and privileged mode of the CPU.
 *
 * param   None
 *
 * return  None
 *
 **/
void IntProtectionDisable(void)
{
    HWREG(SOC_AINTC_REGS + INTC_PROTECTION) &= ~INTC_PROTECTION_PROTECTION;
}

/**
 * Enables the free running of input synchronizer clock
 *
 * param   None
 *
 * return  None
 *
 **/
void IntSyncClkFreeRunSet(void)
{
    HWREG(SOC_AINTC_REGS + INTC_IDLE) &= ~INTC_IDLE_TURBO;
}

/**
 * When this API is called, Input synchronizer clock is auto-gated 
 * based on interrupt input activity
 *
 * param   None
 *
 * return  None
 *
 **/
void IntSyncClkAutoGateSet(void)
{
    HWREG(SOC_AINTC_REGS + INTC_IDLE) |= INTC_IDLE_TURBO;
}

/**
 * Enables the free running of functional clock
 *
 * param   None
 *
 * return  None
 *
 **/
void IntFuncClkFreeRunSet(void)
{
    HWREG(SOC_AINTC_REGS + INTC_IDLE) |= INTC_IDLE_FUNCIDLE;
}

/**
 * When this API is called, functional clock gating strategy
 * is applied.
 *
 * param   None
 *
 * return  None
 *
 **/
void IntFuncClkAutoGateSet(void)
{
    HWREG(SOC_AINTC_REGS + INTC_IDLE) &= ~INTC_IDLE_FUNCIDLE;
}

/**
 * Returns the currently active IRQ priority level.
 *
 * param   None
 *
 * return  Current IRQ priority 
 *
 **/
unsigned int IntCurrIrqPriorityGet(void)
{
    return (HWREG(SOC_AINTC_REGS + INTC_IRQ_PRIORITY) 
            & INTC_IRQ_PRIORITY_IRQPRIORITY);
}

/**
 * Returns the priority threshold.
 *
 * param   None
 *
 * return  Priority threshold value.
 *
 **/
unsigned int IntPriorityThresholdGet(void)
{
    return (HWREG(SOC_AINTC_REGS + INTC_THRESHOLD) 
            & INTC_THRESHOLD_PRIORITYTHRESHOLD);
}

/**
 * Sets the given priority threshold value. 
 *
 * param   threshold - Priority threshold value
 *
 *      'threshold' can take any value from 0x00 to 0x7F. To disable
 *      priority threshold, write 0xFF.
 *             
 * return  None.
 *
 **/
void IntPriorityThresholdSet(unsigned int threshold)
{
    HWREG(SOC_AINTC_REGS + INTC_THRESHOLD) = 
                     threshold & INTC_THRESHOLD_PRIORITYTHRESHOLD;
}

/**
 * Returns the raw interrupt status before masking.
 *
 * param   intrNum - the system interrupt number.
 *
 * return  true - if the raw status is set \n
 *         false - if the raw status is not set.   
 *
 **/
unsigned int IntRawStatusGet(unsigned int intrNum)
{
    return ((0 == ((HWREG(SOC_AINTC_REGS + INTC_ITR(intrNum >> REG_IDX_SHIFT))
                    >> (intrNum & REG_BIT_MASK))& 0x01)) ? false : true);
}

/**
 * Sets software interrupt for the given interrupt number.
 *
 * param   intrNum - the system interrupt number, for which software interrupt
 *                   to be generated
 *
 * return  None
 *
 **/
void IntSoftwareIntSet(unsigned int intrNum)
{
    /* Enable the software interrupt in the corresponding ISR_SET register */
    HWREG(SOC_AINTC_REGS + INTC_ISR_SET(intrNum >> REG_IDX_SHIFT))
                                   = (0x01 << (intrNum & REG_BIT_MASK));

}

/**
 * Clears the software interrupt for the given interrupt number.
 *
 * param   intrNum - the system interrupt number, for which software interrupt
 *                   to be cleared.
 *
 * return  None
 *
 **/
void IntSoftwareIntClear(unsigned int intrNum)
{
    /* Disable the software interrupt in the corresponding ISR_CLEAR register */
    HWREG(SOC_AINTC_REGS + INTC_ISR_CLEAR(intrNum >> REG_IDX_SHIFT))
                                   = (0x01 << (intrNum & REG_BIT_MASK));

}

/**
 * Returns the IRQ status after masking.
 *
 * param   intrNum - the system interrupt number
 *
 * return  true - if interrupt is pending \n
 *         false - in no interrupt is pending
 *
 **/
unsigned int IntPendingIrqMaskedStatusGet(unsigned int intrNum)
{
    return ((0 ==(HWREG(SOC_AINTC_REGS + INTC_PENDING_IRQ(intrNum >> REG_IDX_SHIFT))
                  >> (((intrNum & REG_BIT_MASK)) & 0x01))) ? false : true);
}

/**
 * Enables the processor IRQ only in CPSR. Makes the processor to 
 * respond to IRQs.  This does not affect the set of interrupts 
 * enabled/disabled in the AINTC.
 *
 * param    None
 *
 * return   None
 *
 *  Note: This function call shall be done only in previleged mode of ARM
 **/
void IntMasterIRQEnable(void)
{
    /* Enable IRQ in CPSR.*/
    CPUirqe();
}

/**
 * Disables the processor IRQ only in CPSR.Prevents the processor to 
 * respond to IRQs.  This does not affect the set of interrupts 
 * enabled/disabled in the AINTC.
 *
 * param    None
 *
 * return   None
 *
 *  Note: This function call shall be done only in previleged mode of ARM
 **/
void IntMasterIRQDisable(void)
{
    /* Disable IRQ in CPSR.*/
    CPUirqd();
}

/**
 * Enables the processor FIQ only in CPSR. Makes the processor to 
 * respond to FIQs.  This does not affect the set of interrupts 
 * enabled/disabled in the AINTC.
 *
 * param    None
 *
 * return   None
 *
 *  Note: This function call shall be done only in previleged mode of ARM
 **/
void IntMasterFIQEnable(void)
{
    /* Enable FIQ in CPSR.*/
    CPUfiqe();
}

/**
 * Disables the processor FIQ only in CPSR.Prevents the processor to 
 * respond to FIQs.  This does not affect the set of interrupts 
 * enabled/disabled in the AINTC.
 *
 * param    None
 *
 * return   None
 *
 *  Note: This function call shall be done only in previleged mode of ARM
 **/
void IntMasterFIQDisable(void)
{
    /* Disable FIQ in CPSR.*/
    CPUfiqd();
}

/**
 * Returns the status of the interrupts FIQ and IRQ.
 *
 * param    None
 *
 * return   Status of interrupt as in CPSR.
 *
 *  Note: This function call shall be done only in previleged mode of ARM
 **/
unsigned int IntMasterStatusGet(void)
{
    return CPUIntStatus();
}

/**
 * Read and save the stasus and Disables the processor IRQ .
 * Prevents the processor to respond to IRQs.  
 *
 * param    None
 *
 * return   Current status of IRQ
 *
 *  Note: This function call shall be done only in previleged mode of ARM
 **/
unsigned char IntDisable(void)
{
    unsigned char status;

    /* Reads the current status.*/
    status = (IntMasterStatusGet() & 0xFF);

    /* Disable the Interrupts.*/
    IntMasterIRQDisable();

    return status;
}

/**
 * Restore the processor IRQ only status. This does not affect 
 * the set of interrupts enabled/disabled in the AINTC.
 *
 * param    The status returned by the IntDisable fundtion.
 *
 * return   None
 *
 *  Note: This function call shall be done only in previleged mode of ARM
 **/
void IntEnable(unsigned char  status)
{
    if((status & 0x80) == 0) 
    {
        IntMasterIRQEnable();
    } 
}

