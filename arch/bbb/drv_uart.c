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
#include "drv_uart.h"
#include "hw_types.h"

/*****************************************************************************
**                FUNCTION DEFINITIONS
*****************************************************************************/

/**
 * This API configures the operating mode for the UART instance.
 *       The different operating modes are:
 *       - UART(16x, 13x, 16x Auto-Baud)
 *       - IrDA(SIR, MIR, FIR)
 *       - CIR
 *       - Disabled state(default state)
 *
 * baseAdd   Memory address of the UART instance being used.
 * modeFlag  A value which holds the mode number. This mode  
 *          number is referred from the MODESELECT field in MDR1.
 *
 * 'modeFlag' can take one of the following values:
 * - UART16x_OPER_MODE - to switch to UART 16x operating mode
 * - UART_SIR_OPER_MODE - to swith to IrDA SIR operating mode
 * - UART16x_AUTO_BAUD_OPER_MODE - to switch to UART 16x Auto Baud operating
 *   mode
 * - UART13x_OPER_MODE - to switch to UART 13x operating mode
 * - UART_MIR_OPER_MODE - to switch to IrDA MIR operating mode 
 * - UART_FIR_OPER_MODE - to switch to IrDA FIR operating mode
 * - UART_CIR_OPER_MODE - to switch to CIR operating mode
 * - UART_DISABLED_MODE - to switch to Disabled state
 *
 * return  The mode number in the MODESELECT field of MDR1 before it
 *          was  modified.
 */

unsigned int 
UARTOperatingModeSelect(unsigned int baseAdd,
			unsigned int modeFlag)
{
    unsigned int operMode = 0;

    operMode = (HWREG(baseAdd + UART_MDR1) & UART_MDR1_MODE_SELECT);

    /* Clearing the MODESELECT field in MDR1. */
    HWREG(baseAdd + UART_MDR1) &= ~(UART_MDR1_MODE_SELECT);
    /* Programming the MODESELECT field in MDR1. */
    HWREG(baseAdd + UART_MDR1) |= (modeFlag & UART_MDR1_MODE_SELECT);

    return operMode;
}

/**
 * This API computes the divisor value for the specified operating
 * mode. Not part of this API, the divisor value returned is written
 * to the Divisor Latches to configure the Baud Rate.
 *
 * moduleClk        The frequency of the input clock to the UART module
 * modeFlag         A value which represents the current operating mode
 * baudRate         The required baud rate of communication in bits
 *                  per second(bps)
 * mirOverSampRate  Over-sampling rate for MIR mode.This is applicable
 *                  only when MIR mode of operation is chosen.
 *                  Otherwise, this value is not considered.
 *
 * 'modeFlag' can take one of the following values:
 * - UART16x_OPER_MODE - indicating 16x operating mode
 * - UART13x_OPER_MODE - indicating 13x operating mode 
 * - UART_SIR_OPER_MODE - indicating SIR operating mode
 * - UART_MIR_OPER_MODE - indicating MIR operating mode
 * - UART_FIR_OPER_MODE - indicating FIR operating mode
 *
 * 'mirOverSampRate' can take one of the following values:
 * - UART_MIR_OVERSAMPLING_RATE_41 - for an over-sampling rate of 41
 * - UART_MIR_OVERSAMPLING_RATE_42 - for an over-smapling rate of 42
 *
 * return The divisor value computed for the specified mode.
 *
 * Refer to the section in the user guide that specifies the baud rate
 * computation method to find the supported values of baud rates.
 */

unsigned int 
UARTDivisorValCompute(unsigned int moduleClk,
		      unsigned int baudRate,
		      unsigned int modeFlag,
		      unsigned int mirOverSampRate)
{
    (void)mirOverSampRate;
    unsigned int divisorValue = 0;

    modeFlag &= UART_MDR1_MODE_SELECT;

    switch(modeFlag) {
    case UART16x_OPER_MODE:
    case UART_SIR_OPER_MODE:
	divisorValue = (moduleClk)/(16 * baudRate);
        break;

    case UART13x_OPER_MODE:
	divisorValue = (moduleClk)/(13 * baudRate);
        break;

    case UART_MIR_OPER_MODE:
	divisorValue = (moduleClk)/(mirOverSampRate * baudRate);
        break;

    case UART_FIR_OPER_MODE:
	divisorValue = 0;
        break;

    default:
        break;
    }

    return divisorValue;
}

/**
 * This API is used to write the specified divisor value to Divisor
 * Latch registers DLL and DLH.
 *
 * baseAdd       Memory address of the UART instance being used.
 * divisorValue  The 14-bit value whose least 8 bits go to DLL
 *               and highest 6 bits go to DLH.
 * 
 * return A concatenated value of DLH and DLL registers(DLH:DLL, a 14-bit 
 * value) before they are modified in the current API.
 */

unsigned int
UARTDivisorLatchWrite(unsigned int baseAdd, 
		      unsigned int divisorValue)
{
    volatile unsigned int enhanFnBitVal = 0;
    volatile unsigned int sleepMdBitVal = 0;
    volatile unsigned int lcrRegValue = 0;
    volatile unsigned int operMode = 0;
    unsigned int divRegVal = 0;

    /* Switching to Register Configuration Mode B. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Collecting the current value of EFR[4] and later setting it. */
    enhanFnBitVal = HWREG(baseAdd + UART_EFR) & UART_EFR_ENHANCED_EN;
    HWREG(baseAdd + UART_EFR) |= UART_EFR_ENHANCED_EN;

    /* Switching to Register Operational Mode. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    /*
    ** Collecting the current value of IER[4](SLEEPMODE bit) and later
    ** clearing it.
    */
    sleepMdBitVal = HWREG(baseAdd + UART_IER) & UART_IER_SLEEP_MODE_IT;
    HWREG(baseAdd + UART_IER) &= ~(UART_IER_SLEEP_MODE_IT);

    /* Switching to Register Configuration Mode B. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Collecting the current value of Divisor Latch Registers. */
    divRegVal = HWREG(baseAdd + UART_DLL) & 0xFF;
    divRegVal |= ((HWREG(baseAdd + UART_DLH) & 0x3F) << 8);

    /* Switch the UART instance to Disabled state. */
    operMode = UARTOperatingModeSelect(baseAdd, UART_MDR1_MODE_SELECT_DISABLED);

    /* Writing to Divisor Latch Low(DLL) register. */
    HWREG(baseAdd + UART_DLL) = (divisorValue & 0x00FF);

    /* Writing to Divisor Latch High(DLH) register. */
    HWREG(baseAdd + UART_DLH) = ((divisorValue & 0x3F00) >> 8);

    /* Restoring the Operating Mode of UART. */
    UARTOperatingModeSelect(baseAdd, operMode);

    /* Switching to Register Operational Mode. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    /* Restoring the value of IER[4] to its original value. */
    HWREG(baseAdd + UART_IER) |= sleepMdBitVal;

    /* Switching to Register Configuration Mode B. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Restoring the value of EFR[4] to its original value. */
    HWREG(baseAdd + UART_EFR) &= ~(UART_EFR_ENHANCED_EN);
    HWREG(baseAdd + UART_EFR) |= enhanFnBitVal;

    /* Restoring the value of LCR Register. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return divRegVal;
}

/**
 * This API enables write access to Divisor Latch registers DLL and
 * DLH.
 *
 * baseAdd   Memory address of the UART instance being used.
 *
 * return None.
 */

void 
UARTDivisorLatchEnable(unsigned int baseAdd)
{
    /* Enable access to Divisor Latch registers by setting LCR[7] bit. */
    HWREG(baseAdd + UART_LCR) |= (UART_LCR_DIV_EN);
}

/**
 * This API disables write access to Divisor Latch registers DLL and
 * DLH.
 *
 * baseAdd   Memory address of the UART instance being used.
 *
 * return None.
 *
 * Disabling write access to Divisor Latch Registers enables access to
 * MCR, FCR, IER, BLR, EBLR, RHR registers.
 */
void 
UARTDivisorLatchDisable(unsigned int baseAdd)
{
    /* Disabling access to Divisor Latch registers by clearing LCR[7] bit. */
    HWREG(baseAdd + UART_LCR) &= ~(UART_LCR_DIV_EN);
}


/**
 * This API configures the specified Register Configuration mode for
 * the UART.
 *
 * baseAdd   Memory address of the UART instance being used.
 * modeFlag  This specifies the register configuration mode to be
 *           enabled.
 *
 * 'modeFlag' can take one of the following values:
 * - UART_REG_CONFIG_MODE_A - to enable Register Configuration Mode A
 * - UART_REG_CONFIG_MODE_B - to enable Register Configuration Mode B
 * - UART_REG_OPERATIONAL_MODE - to enable Register Operational Mode
 *
 * return The contents of the Line Control Register(LCR) before it was
 *         modified.
 *
 * Since the UART module registers that can be accessed at any time
 * depends on the value in the Line Control Register(LCR), three
 * register configuration modes have been defined, each corresponding
 * to a specific state of the LCR. The three register configuration
 * modes are:
 *  - Configuration Mode A: LCR[7] = 1 and LCR[7:0] != 0xBF.
 *  - Configuration Mode B: LCR[7:0] = 0xBF.
 *  - Operational Mode    : LCR[7] = 0.
 *
 *  Refer to the Register Listing in the UART/IrDA/CIR peripheral
 *  document for more information.
 *
 */

unsigned int 
UARTRegConfigModeEnable(unsigned int baseAdd, unsigned int modeFlag)
{
    unsigned int lcrRegValue = 0;

    /* Preserving the current value of LCR. */
    lcrRegValue = HWREG(baseAdd + UART_LCR);

    switch(modeFlag)
    {
    case UART_REG_CONFIG_MODE_A:
    case UART_REG_CONFIG_MODE_B:
	HWREG(baseAdd + UART_LCR) = (modeFlag & 0xFF);
        break;

    case UART_REG_OPERATIONAL_MODE:
	HWREG(baseAdd + UART_LCR) &= 0x7F;
        break;

    default:
        break;
    }

    return lcrRegValue;
}


/**
 * This API is used to restore the UART to the specified Register
 * Configuration Mode. 
 *
 * baseAdd      Memory address of the UART instance being used.
 * lcrRegValue  The value to be loaded to the Line Control Register(LCR).
 *
 * return None
 *
 * The API UARTRegConfigModeEnable() and the current API are used
 * hand-in-hand. While UARTRegConfigModeEnable() switches the UART to
 * the requested operating mode, the current API restores the UART to
 * that register configuration mode prevalent before
 * UARTRegConfigModeEnable() was called.
 */

void 
UARTRegConfModeRestore(unsigned int baseAdd, unsigned int lcrRegValue)
{
    /* Programming the Line Control Register(LCR). */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;
}

/**
 * This API is used to introduce or to remove a Break condition.
 * 
 * baseAdd     Memory address of the UART instance being used.
 * breakState  This specifies whether the break condition should be
 *             introduced or removed.
 *
 * 'breakState' can take one of the following two values:
 * - UART_BREAK_COND_DISABLE - to disable the Break condition if it has
 *   already been enabled
 * - UART_BREAK_COND_ENABLE - to enable the Break condition
 *
 * return None
 *
 * When the Break Condition is imposed, the Transmitter output line TX
 * goes low to alert the communication terminal.
 */

void 
UARTBreakCtl(unsigned int baseAdd, unsigned int breakState)
{
    /* Clearing the BREAK_EN bit in LCR. */
    HWREG(baseAdd + UART_LCR) &= ~(UART_LCR_BREAK_EN);

    /* Programming the BREAK_EN bit in LCR. */
    HWREG(baseAdd + UART_LCR) |= (breakState & UART_LCR_BREAK_EN);
}

/**
 * This API configures the Line Characteristics for the
 * UART instance. The Line Characteristics include:
 *  - Word length per frame
 *  - Number of Stop Bits per frame
 *  - Parity feature configuration
 *
 * baseAdd     Memory address of the UART instance being used.
 * wLenStbFlag Bit mask value of the bits pertaining to word
 *             length selection and stop bits selection in LCR.
 * parityFlag  Bit mask value of the bits pertaining to parity
 *             feature selection in LCR.
 *
 * 'wLenStbFlag' can take one of the following macros:
 *  - (UART_FRAME_WORD_LENGTH_n | UART_FRAME_NUM_STB_1), where
 *    n = 5,6,7 or 8. -- This signifies that 1 stop bit and
 *    one among 5,6,7 and 8 bits are chosen as the word length
 *    per frame.
 *  - (UART_FRAME_WORD_LENGTH_n | UART_FRAME_NUM_STB_1_5_2), where
 *    n = 5,6,7 or 8. -- This signifies that the word length and
 *    number of stop bits per frame could be one among the below
 *    four choices:
 *    --- WL = 5          NUM_STB = 1.5
 *    --- WL = 6,7 or 8   NUM_STB = 2
 *
 * 'parityFlag' can take one of the following macros:
 *  - (UART_ODD_PARITY) - signifying that odd parity be enabled and the
 *     parity bit be represented in a default manner.
 *  - (UART_EVEN_PARITY) - signifying that even parity be enabled and the
 *     parity bit be represented in a default manner.
 *  - (UART_PARITY_REPR_1) - signifying that the parity bit be represented
 *     by a logic 1 in the transmitted and received data.
 *  - (UART_PARITY_REPR_0) - signifying that the parity bit be represented
 *     by a logic 0 in the transmitted and received data.
 *  - (UART_PARITY_NONE) - signifying that no parity be enabled.
 *
 * return  None.
 */
 
void 
UARTLineCharacConfig(unsigned int baseAdd, 
		     unsigned int wLenStbFlag,
		     unsigned int parityFlag)
{
    /* Clearing the CHAR_LENGTH and NB_STOP fields in LCR.*/
    HWREG(baseAdd + UART_LCR) &= ~(UART_LCR_NB_STOP | UART_LCR_CHAR_LENGTH);
    /* Programming the CHAR_LENGTH and NB_STOP fields in LCR. */
    HWREG(baseAdd + UART_LCR) |= (wLenStbFlag &
                                  (UART_LCR_NB_STOP | UART_LCR_CHAR_LENGTH));

    /* Clearing the PARITY_EN, PARITY_TYPE1 and PARITY_TYPE2 fields in LCR. */
    HWREG(baseAdd + UART_LCR) &= ~(UART_LCR_PARITY_TYPE2 |
                                   UART_LCR_PARITY_TYPE1 |
                                   UART_LCR_PARITY_EN);
    /* Programming the PARITY_EN, PARITY_TYPE1 and PARITY_TYPE2 fields in LCR.*/
    HWREG(baseAdd + UART_LCR) |= (parityFlag & (UART_LCR_PARITY_TYPE2 |
                                                UART_LCR_PARITY_TYPE1 |
                                                UART_LCR_PARITY_EN));

}

/**
 * This API configures the Parity feature for the UART.
 *
 * baseAdd     Memory address of the UART instance being used
 * parityFlag  This specifies the parity configuration to be
 *             programmed to the Line Control Register(LCR)
 *
 * 'parityFlag' can take one of the following values:
 *  - (UART_ODD_PARITY) - signifying that odd parity be enabled and the parity
 *     bit be represented in a default manner
 *  - (UART_EVEN_PARITY) - signifying that even parity be enabled and the
 *     parity bit be represented in a default manner
 *  - (UART_ODD_PARITY_REPR_1) - signifying that odd parity be enabled and the
 *     parity bit be represented by a logic 1
 *  - (UART_EVEN_PARITY_REPR_0)- signifying that even parity be enabled and the
 *     parity bit be represented by a logic 0
 *  - (UART_PARITY_NONE) - signifying that no parity be enabled
 *
 * return None
 */

void 
UARTParityModeSet(unsigned int baseAdd, unsigned int parityFlag)
{
    HWREG(baseAdd + UART_LCR) &= ~((UART_LCR_PARITY_TYPE2 |
                                    UART_LCR_PARITY_TYPE1 |
                                    UART_LCR_PARITY_EN));

    /* Programming the PARITY_TYPE2, PARITY_TYPE1 and PARITY_EN fields of LCR. */
    HWREG(baseAdd + UART_LCR) |= (parityFlag & (UART_LCR_PARITY_TYPE2 |
                                                UART_LCR_PARITY_TYPE1 |
                                                UART_LCR_PARITY_EN));
}

/**
 * This API reads the Parity configuration being set in the UART.
 *
 * baseAdd     Memory address of the UART instance being used
 *
 * return This returs one of the following values:
 *  - (UART_ODD_PARITY) - signifying that odd parity is enabled and the parity
 *     bit is represented in a default manner
 *  - (UART_EVEN_PARITY) - signifying that even parity is enabled and the
 *     parity bit is represented in a default manner
 *  - (UART_ODD_PARITY_REPR_1) - signifying that odd parity is enabled and the
 *     parity bit is represented by a logic 1
 *  - (UART_EVEN_PARITY_REPR_0)- signifying that even parity is enabled and the
 *     parity bit is represented by a logic 0
 *  - (UART_PARITY_NONE) - signifying that no parity is enabled
 *
 */

unsigned int 
UARTParityModeGet(unsigned int baseAdd)
{
    return (HWREG(baseAdd + UART_LCR) & (UART_LCR_PARITY_TYPE2 |
                                         UART_LCR_PARITY_TYPE1 |
                                         UART_LCR_PARITY_EN));
}

/**
 * This API configures the FIFO settings for the UART instance.
 *   Specifically, this does the following configurations:
 *     1> Configures the Transmitter and Receiver FIFO Trigger Level
 *        granularity
 *     2> Configures the Transmitter and Receiver FIFO Trigger Level
 *     3> Configures the bits which clear/not clear the TX and RX FIFOs
 *     4> Configures the DMA mode of operation
 * 
 * baseAdd       Memory address of the UART instance being used.
 * fifoConfig    This specifies the desired FIFO configurations.
 *               Use the macro UART_FIFO_CONFIG to pass the required
 *               FIFO settings.
 *
 * The parameters of UART_FIFO_CONFIG can take the following values:
 * -- txGra - the Transmitter FIFO trigger level granularity
 * -- rxGra - the Receiver FIFO trigger level granularity
 *    These can take one of the following two values:
 *    - UART_TRIG_LVL_GRANULARITY_1 - for a granularity of 1,
 *    - UART_TRIG_LVL_GRANULARITY_4 - for a granularity of 4.
 *
 * -- txTrig - the Transmitter FIFO trigger level
 *
 * For 'txGra' being UART_TRIG_LVL_GRANULARITY_4, this can take one of the
 * values from one of the following two sets:
 * Set 1> UART_FCR_TX_TRIG_LVL_n, where n = 8,16,32,56.
 *        These are programmed to the FCR[5:4] in FCR register.
 * Set 2> m , where (1 <= m <= 15).
 *        The trigger level would then be (m * 4).
 *        For example: If m = 9, then trigger level = 36.
 *        The value 'm' is programmed into TLR[3:0] field in TLR register.
 * 
 * For granularity being UART_TRIG_LVL_GRANULARITY_1, this can take
 * any decimal value from 1 to 63.
 * 
 * -- rxTrig - the Receiver FIFO Trigger level
 *
 * For granularity being UART_TRIG_LVL_GRANULARITY_4, this can
 * take one of the macro from one of the following two sets:
 * Set 1> UART_FCR_RX_TRIG_LVL_n, where n = 8,16,56,60.  
 *        These are programmed into FCR[7:6] field in FCR register.
 * Set 2> m , where (1 <= m <= 15).
 *        The trigger level would then be (m * 4).
 *        For example: If m = 7, then trigger level = 28.
 *        The value 'm' is programmed to the TLR[7:4] field of TLR.
 * 
 * For granularity being UART_TRIG_LVL_GRANULARITY_1, this can take
 * any decimal value from 1 to 63.
 *
 * -- txClr - the Transmitter FIFO clear flag
 * -- rxClr - the Receiver FIFO clear flag
 *   
 * These can take the following values:
 * 0 - to not clear the FIFO
 * 1 - to clear the FIFO. Clearing the FIFO also results in resetting the
 *     FIFO counter logic to 0.
 *
 * -- dmaEnPath - specifies whether DMA Mode settings should be configured
 *    through FCR or SCR. This can take one of the following two values:
 *    UART_DMA_EN_PATH_FCR - to configure DMA through FCR
 *    UART_DMA_EN_PATH_SCR - to configure DMA through SCR 
 *
 * -- dmaMode - specifies the DMA Mode to be used
 *    This can take one of the following four values:
 * - UART_DMA_MODE_m_ENABLE, where m = 0, 1, 2 or 3.
 *
 * return  The value that was written to the FIFO Control Register(FCR).
 *
 *   1> FIFO Control Register(FCR) is a write-only register and its
 *      contents cannot be read. Hence, it is  not possible for a
 *      read-modify-write operation on this register. Therefore it is
 *      expected that the FIFO configuration is done at once for both
 *      TX and RX directions and always the entire FCR related information is
 *      required during modification of the FIFO settings.
 *      Also the FIFO related settings are valid only when the FIFO is
 *      enabled. This means that FIFO mode of operation should be enabled for
 *      FIFO related settings to take effect.
 */

unsigned int 
UARTFIFOConfig(unsigned int baseAdd,
	       unsigned int fifoConfig)
{
    unsigned int txGra = (fifoConfig & UART_FIFO_CONFIG_TXGRA) >> 26;
    unsigned int rxGra = (fifoConfig & UART_FIFO_CONFIG_RXGRA) >> 22;

    unsigned int txTrig = (fifoConfig & UART_FIFO_CONFIG_TXTRIG) >> 14;
    unsigned int rxTrig = (fifoConfig & UART_FIFO_CONFIG_RXTRIG) >> 6;

    unsigned int txClr = (fifoConfig & UART_FIFO_CONFIG_TXCLR) >> 5;
    unsigned int rxClr = (fifoConfig & UART_FIFO_CONFIG_RXCLR) >> 4;

    unsigned int dmaEnPath = (fifoConfig & UART_FIFO_CONFIG_DMAENPATH) >> 3;
    unsigned int dmaMode = (fifoConfig & UART_FIFO_CONFIG_DMAMODE);

    unsigned int tcrTlrBitVal = 0;
    unsigned int tlrValue = 0;
    unsigned int fcrValue = 0;

    tcrTlrBitVal = UARTSubConfigTCRTLRModeEn(baseAdd);

    /* Enabling FIFO mode of operation. */
    fcrValue |= UART_FCR_FIFO_EN;

    /* Setting the Receiver FIFO trigger level. */
    if(UART_TRIG_LVL_GRANULARITY_1 != rxGra)
    {
        /* Clearing the RXTRIGGRANU1 bit in SCR. */
        HWREG(baseAdd + UART_SCR) &= ~(UART_SCR_RX_TRIG_GRANU1);

        /* Clearing the RX_FIFO_TRIG_DMA field of TLR register. */
        HWREG(baseAdd + UART_TLR) &= ~(UART_TLR_RX_FIFO_TRIG_DMA);

        fcrValue &= ~(UART_FCR_RX_FIFO_TRIG);
        
        /*
        ** Checking if 'rxTrig' matches with the RX Trigger level values
        ** in FCR.
        */
        if((UART_FCR_RX_TRIG_LVL_8 == rxTrig)  ||
           (UART_FCR_RX_TRIG_LVL_16 == rxTrig) ||
           (UART_FCR_RX_TRIG_LVL_56 == rxTrig) ||
           (UART_FCR_RX_TRIG_LVL_60 == rxTrig))
        {
            fcrValue |= (rxTrig & UART_FCR_RX_FIFO_TRIG);
        }
        else
        {
            /* RX Trigger level will be a multiple of 4. */
            /* Programming the RX_FIFO_TRIG_DMA field of TLR register. */
            HWREG(baseAdd + UART_TLR) |= ((rxTrig <<
                                           UART_TLR_RX_FIFO_TRIG_DMA_SHIFT) &
                                           UART_TLR_RX_FIFO_TRIG_DMA);
        }
    }
    else
    {
        /* 'rxTrig' now has the 6-bit RX Trigger level value. */

        rxTrig &= 0x003F;

        /* Collecting the bits rxTrig[5:2]. */
        tlrValue = (rxTrig & 0x003C) >> 2;

        /* Collecting the bits rxTrig[1:0] and writing to 'fcrValue'. */
        fcrValue |= (rxTrig & 0x0003) << UART_FCR_RX_FIFO_TRIG_SHIFT;
        
        /* Setting the RXTRIGGRANU1 bit of SCR register. */
        HWREG(baseAdd + UART_SCR) |= UART_SCR_RX_TRIG_GRANU1;

        /* Programming the RX_FIFO_TRIG_DMA field of TLR register. */
        HWREG(baseAdd + UART_TLR) |= (tlrValue << UART_TLR_RX_FIFO_TRIG_DMA_SHIFT);

    }

    /* Setting the Transmitter FIFO trigger level. */
    if(UART_TRIG_LVL_GRANULARITY_1 != txGra)
    {
        /* Clearing the TXTRIGGRANU1 bit in SCR. */
        HWREG(baseAdd + UART_SCR) &= ~(UART_SCR_TX_TRIG_GRANU1);

        /* Clearing the TX_FIFO_TRIG_DMA field of TLR register. */
        HWREG(baseAdd + UART_TLR) &= ~(UART_TLR_TX_FIFO_TRIG_DMA);

        fcrValue &= ~(UART_FCR_TX_FIFO_TRIG);

        /*
        ** Checking if 'txTrig' matches with the TX Trigger level values
        ** in FCR.
        */
        if((UART_FCR_TX_TRIG_LVL_8 == (txTrig))  ||
           (UART_FCR_TX_TRIG_LVL_16 == (txTrig)) ||
           (UART_FCR_TX_TRIG_LVL_32 == (txTrig)) ||
           (UART_FCR_TX_TRIG_LVL_56 == (txTrig)))
        {
            fcrValue |= (txTrig & UART_FCR_TX_FIFO_TRIG);
        }
        else
        {
            /* TX Trigger level will be a multiple of 4. */
            /* Programming the TX_FIFO_TRIG_DMA field of TLR register. */
            HWREG(baseAdd + UART_TLR) |= ((txTrig <<
                                           UART_TLR_TX_FIFO_TRIG_DMA_SHIFT) &
                                           UART_TLR_TX_FIFO_TRIG_DMA);
        }
    }
    else
    {
        /* 'txTrig' now has the 6-bit TX Trigger level value. */

        txTrig &= 0x003F;

        /* Collecting the bits txTrig[5:2]. */
        tlrValue = (txTrig & 0x003C) >> 2;

        /* Collecting the bits txTrig[1:0] and writing to 'fcrValue'. */
        fcrValue |= (txTrig & 0x0003) << UART_FCR_TX_FIFO_TRIG_SHIFT;
        
        /* Setting the TXTRIGGRANU1 bit of SCR register. */
        HWREG(baseAdd + UART_SCR) |= UART_SCR_TX_TRIG_GRANU1;

        /* Programming the TX_FIFO_TRIG_DMA field of TLR register. */
        HWREG(baseAdd + UART_TLR) |= (tlrValue << UART_TLR_TX_FIFO_TRIG_DMA_SHIFT);
    }

    if(UART_DMA_EN_PATH_FCR == dmaEnPath)
    {
        /* Configuring the UART DMA Mode through FCR register. */
        HWREG(baseAdd + UART_SCR) &= ~(UART_SCR_DMA_MODE_CTL);

        dmaMode &= 0x1;

        /* Clearing the bit corresponding to the DMA_MODE in 'fcrValue'. */
        fcrValue &= ~(UART_FCR_DMA_MODE);

        /* Setting the DMA Mode of operation. */
        fcrValue |= (dmaMode << UART_FCR_DMA_MODE_SHIFT);
    }
    else
    {
        dmaMode &= 0x3;

        /* Configuring the UART DMA Mode through SCR register. */
        HWREG(baseAdd + UART_SCR) |= UART_SCR_DMA_MODE_CTL;

        /* Clearing the DMAMODE2 field in SCR. */
        HWREG(baseAdd + UART_SCR) &= ~(UART_SCR_DMA_MODE_2);

        /* Programming the DMAMODE2 field in SCR. */
        HWREG(baseAdd + UART_SCR) |= (dmaMode << UART_SCR_DMA_MODE_2_SHIFT);
    }

    /* Programming the bits which clear the RX and TX FIFOs. */
    fcrValue |= (rxClr << UART_FCR_RX_FIFO_CLEAR_SHIFT);
    fcrValue |= (txClr << UART_FCR_TX_FIFO_CLEAR_SHIFT);

    /* Writing 'fcrValue' to the FIFO Control Register(FCR). */
    UARTFIFORegisterWrite(baseAdd, fcrValue);

    /* Restoring the value of TCRTLR bit in MCR. */
    UARTTCRTLRBitValRestore(baseAdd, tcrTlrBitVal);

    return fcrValue;
}

/**
 * This API enables the DMA mode of operation for the UART instance.
 *
 * baseAdd      Memory address of the UART instance being used.
 * dmaModeFlag  This specifies the DMA mode to be enabled for the
 *              UART instance.
 *
 * 'dmaModeFlag' can take one of the following four values:
 * - UART_DMA_MODE_0_ENABLE - to enable DMA Mode 0(No DMA)
 * - UART_DMA_MODE_1_ENABLE - to enable DMA Mode 1(DMA for both TX and RX)
 * - UART_DMA_MODE_2_ENABLE - to enable DMA Mode 2(DMA only for RX)
 * - UART_DMA_MODE_3_ENABLE - to enable DMA Mode 3(DMA only for TX)
 *
 * return  None
 *
 * This API enables the specified DMA modes always in SCR and FCR is
 * not used. This is because SCR[2:1] allows the program to enable any
 * of the four available DMA modes while FCR[3] allows the program to
 * use either DMA Mode 0 or DMA Mode 1.
 */

void 
UARTDMAEnable(unsigned int baseAdd, unsigned int dmaModeFlag)
{
    /* Setting the DMAMODECTL bit in SCR to 1. */
    HWREG(baseAdd + UART_SCR) |= (UART_SCR_DMA_MODE_CTL);

    /* Clearing the DMAMODE2 field in SCR. */
    HWREG(baseAdd + UART_SCR) &= ~(UART_SCR_DMA_MODE_2);

    /* Programming the DMAMODE2 field in SCR. */
    HWREG(baseAdd + UART_SCR) |= ((dmaModeFlag << UART_SCR_DMA_MODE_2_SHIFT) &
                                   UART_SCR_DMA_MODE_2);
}

/**
 * This API disables the DMA mode of operation.
 *
 * baseAdd   Memory address of the UART instance being used.
 *
 * return None
 */

void 
UARTDMADisable(unsigned int baseAdd)
{
    HWREG(baseAdd + UART_SCR) |= (UART_SCR_DMA_MODE_CTL);
    /* Programming DMAMODE2 field of SCR to DMA mode 0. */
    HWREG(baseAdd + UART_SCR) &= ~(UART_SCR_DMA_MODE_2);
}

/**
 * This API is used to write a specified value to the FIFO Control
 * Register(FCR).
 *
 * baseAdd     Memory address of the UART instance being used.
 * fcrValue    This specifies the value to be written to the
 *             FCR.
 *
 * 'fcrValue' can be determined using a parameterized macro named
 * 'UART_FCR_PROGRAM'.
 * The parameters of 'UART_FCR_PROGRAM' are described below:
 * - rxFIFOTrig - specifies the Receiver FIFO Trigger Level
 * - txFIFOTrig - specifies the Transmitter FIFO Trigger Level
 * - dmaMode - specifies the DMA Mode of operation to be selected:
 *   Write 0 - for DMA Mode 0
 *         1 - for DMA Mode 1
 * - txClr - specifies whether or not to clear the Transmitter FIFO
 *   and resetting the counter logic to 0.
 * - rxClr - specifies whether or not to clear the Receiver FIFO and resetting
 *   the counter logic to 0.
 * - fifoEn - specifies whether to enable the FIFO mode for the UART or not
 *   0 - to enable Non-FIFO mode of operation
 *   1 - to enable FIFO mode of operation
 *
 * return  None
 *
 * 1> The FIFO_EN and DMA_MODE bits of FCR can be written to only when
 *    the Baud Clock is not running(DLL and DLH register are cleared
 *    to 0). Modifying DLL and DLH registers in turn requires that the
 *    UART be operated in Disabled Mode(MDR1[2:0] = 0x7).
 * 2> Writing to 'TX_FIFO_TRIG' field in FCR requires that the
 *    ENHANCEDEN bit in EFR(EFR[4]) be set to 1.
 *    Prior to writing to the FCR, this API does the above two operations.
 *    It also restores the respective bit values after FCR has been
 *    written to.
 */

void 
UARTFIFORegisterWrite(unsigned int baseAdd, unsigned int fcrValue)
{
    unsigned int divLatchRegVal = 0;
    unsigned int lcrRegValue = 0;

    /* Switching to Configuration Mode A. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_A);

    /* Clearing the contents of Divisor Latch Registers. */
    divLatchRegVal = UARTDivisorLatchWrite(baseAdd, 0x0000);

    /* Writing the 'fcrValue' to the FCR register. */
    HWREG(baseAdd + UART_FCR) = fcrValue;

    /* Programming the Divisor Latch Registers with the collected value. */
    UARTDivisorLatchWrite(baseAdd, divLatchRegVal);

    /* Reinstating the value of LCR register. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;
}

/**
 * This API sets a certain bit in Enhanced Feature Register(EFR) which
 * shall avail the UART to use some Enhanced Features.
 *
 * baseAdd    Memory address of the UART instance being used.
 * 
 * return The value of ENHANCEDEN bit in EFR before it is modified in this API.
 *
 * This API switches UART to Configuration Mode B, sets the ENHANCEDEN
 * bit in EFR and reverts the UART back to the original mode of
 * operation.
 *
 */

unsigned int 
UARTEnhanFuncEnable(unsigned int baseAdd)
{
    unsigned int enhanFnBitVal = 0;
    unsigned int lcrRegValue = 0;

    /* Enabling Configuration Mode B of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Collecting the current value of ENHANCEDEN bit of EFR. */
    enhanFnBitVal = (HWREG(baseAdd + UART_EFR) & UART_EFR_ENHANCED_EN);

    /* Setting the ENHANCEDEN bit in EFR register. */
    HWREG(baseAdd + UART_EFR) |= UART_EFR_ENHANCED_EN;

    /* Programming LCR with the collected value. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return enhanFnBitVal;
}

/**
 * This API restores the ENHANCEDEN bit value of EFR register(EFR[4])
 * to the corresponding bit value in 'enhanFnBitVal' passed as a
 * parameter to this API.
 *
 * baseAdd        Memory address of the UART instance being used.
 * enhanFnBitVal  The 4th bit of this 32-bit value shall hold a value
 *                        to which the ENHANCEDEN bit of EFR (EFR[4]) has to
 *                        be restored to.
 * 
 * return None.
 *
 * The APIs UARTEnhanFuncEnable() and the current one are used
 * hand-in-hand. While UARTEnhanFuncEnable() collects the EFR[4] bit
 * value before modifying it, UARTEnhanFuncBitValRestore() can be used
 * to restore EFR[4] bit value after the necessary operation is complete.
 * This API switches the UART to Configuration Mode B, does the needful
 * and reverts it back to original mode of operation.
 *
 */

void 
UARTEnhanFuncBitValRestore(unsigned int baseAdd, 
			   unsigned int enhanFnBitVal)
{
    unsigned int lcrRegValue = 0;

    /* Enabling Configuration Mode B of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Restoring the value of EFR[4]. */
    HWREG(baseAdd + UART_EFR) &= ~(UART_EFR_ENHANCED_EN);
    HWREG(baseAdd + UART_EFR) |= (enhanFnBitVal & UART_EFR_ENHANCED_EN);

    /* Programming LCR with the collected value. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;
}

/**
 * This API enables the MSR_SPR Sub-Configuration Mode of operation.
 *
 * baseAdd   Memory address of the UART instance being used.
 *
 * return The value of the TCRTLR bit in MCR (MCR[6]) before it is modified
 *         in this API.
 *
 * 1> Each Register configuration mode(A, B and operational) has
 *    further sub-configuration modes corresponding to accesses to
 *    offset addresses 0x18 and 0x1C. They are:
 *    For Register Configuration Mode A:
 *    1> MSR_SPR:  EFR[4] = 0  or MCR[6] = 0
 *    2> TCR_TLR:  EFR[4] = 1 and MCR[6] = 1
 *    For Register Configuration Mode B:
 *    1> TCR_TLR - EFR[4] = 1 and MCR[6] = 1
 *    2> XOFF    - EFR[4] = 0 or  MCR[6] = 0
 *
 *    For Register Operational Mode:
 *     1> MSR_SPR - EFR[4] = 0 or  MCR[6] = 0
 *     2> TCR_TLR - EFR[4] = 1 and MCR[6] = 1
 *
 *     In any configuration mode, enabling one of the sub-configuration
 *     mode would disable access to the registers of the other
 *     sub-configuration mode.
 *
 * 2> The current API enables access to Modem Status Register(MSR) and
 *    Scratch Pad Register(SPR). This is applicable for Register
 *    Configuration Mode A and Operational mode of operation.
 */

unsigned int 
UARTSubConfigMSRSPRModeEn(unsigned int baseAdd)
{
    unsigned int enhanFnBitVal = 0;
    unsigned int tcrTlrValue = 0;
    unsigned int lcrRegValue = 0;

    /* Switching to Configuration Mode B of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Collecting the current value of EFR[4] and later setting it. */
    enhanFnBitVal = HWREG(baseAdd + UART_EFR) & UART_EFR_ENHANCED_EN;
    HWREG(baseAdd + UART_EFR) |= UART_EFR_ENHANCED_EN;

    /* Switching to Configuration Mode A of operation. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_A);

    /* Collecting the bit value of MCR[6]. */
    tcrTlrValue = (HWREG(baseAdd + UART_MCR) & UART_MCR_TCR_TLR);

    /* Clearing the TCRTLR bit in MCR register. */
    HWREG(baseAdd + UART_MCR) &= ~(UART_MCR_TCR_TLR);

    /* Switching to Configuration Mode B of operation. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Restoring the value of EFR[4]. */
    HWREG(baseAdd + UART_EFR) &= ~(UART_EFR_ENHANCED_EN);
    HWREG(baseAdd + UART_EFR) |= enhanFnBitVal;

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return tcrTlrValue;
}

/**
 * This API enables the TCR_TLR Sub_Configuration Mode of operation.
 *
 * baseAdd   Memory address of the UART instance being used.
 *
 * return The value of the TCRTLR bit in MCR (MCR[6]) before it is modified
 *         in this API.
 *
 * The current API enables access to Transmission Control Register(TCR)
 * and Trigger Level Register(TLR). This is applicable for all the
 * three register configuration modes.
 * Refer to the comments of UARTSubConfigMSRSPRModeEn() API for more
 * details.
 */

unsigned int 
UARTSubConfigTCRTLRModeEn(unsigned int baseAdd)
{
    unsigned int enhanFnBitVal = 0;
    unsigned int tcrTlrValue = 0;
    unsigned int lcrRegValue = 0;

    /* Switching to Register Configuration Mode B. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Collecting the current value of EFR[4] and later setting it. */
    enhanFnBitVal = HWREG(baseAdd + UART_EFR) & UART_EFR_ENHANCED_EN;
    HWREG(baseAdd + UART_EFR) |= UART_EFR_ENHANCED_EN;

    /* Switching to Register Configuration Mode A. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_A);

    /* Collecting the bit value of MCR[6]. */
    tcrTlrValue = (HWREG(baseAdd + UART_MCR) & UART_MCR_TCR_TLR);

    /* Setting the TCRTLR bit in Modem Control Register(MCR). */
    HWREG(baseAdd + UART_MCR) |= (UART_MCR_TCR_TLR);

    /* Switching to Register Configuration Mode B. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Restoring the value of EFR[4] to its original value. */
    HWREG(baseAdd + UART_EFR) &= ~(UART_EFR_ENHANCED_EN);
    HWREG(baseAdd + UART_EFR) |= enhanFnBitVal;

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return tcrTlrValue;
}

/**
 * This API enables the XOFF Sub-Configuration Mode of operation.
 *
 * baseAdd   Memory address of the UART instance being used.
 *
 * return The value of the TCRTLR bit in MCR (MCR[6]) before it is modified
 *         in this API.
 *
 * The current API enables access to the XOFF (XOFF1 and XOFF2)
 * registers. The XOFF registers can be accessed in Register
 * Configuration Mode B of operation.
 * Refer to the comments of UARTSubConfigMSRSPRModeEn() API for more
 * details.
 */

unsigned int 
UARTSubConfigXOFFModeEn(unsigned int baseAdd)
{
    unsigned int enhanFnBitVal = 0;
    unsigned int tcrTlrValue = 0;
    unsigned int lcrRegValue = 0;

    /* Switching to Register Configuration Mode B. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Collecting the current value of EFR[4] and later setting it. */
    enhanFnBitVal = HWREG(baseAdd + UART_EFR) & UART_EFR_ENHANCED_EN;
    HWREG(baseAdd + UART_EFR) |= UART_EFR_ENHANCED_EN;

    /* Switching to Configuration Mode A of operation. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_A);

    /* Collecting the bit value of TCRTLR(MCR[6]). */
    tcrTlrValue = (HWREG(baseAdd + UART_MCR) & UART_MCR_TCR_TLR);

    /* Clearing the TCRTLR bit in MCR register. */
    HWREG(baseAdd + UART_MCR) &= ~(UART_MCR_TCR_TLR);

    /* Switching to Register Configuration Mode B. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Restoring the value of EFR[4] to its original value. */
    HWREG(baseAdd + UART_EFR) &= ~(UART_EFR_ENHANCED_EN);
    HWREG(baseAdd + UART_EFR) |= enhanFnBitVal;

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return tcrTlrValue;
}

/**
 *   This API restores the TCRTLR bit(MCR[6]) value in Modem Control
 *         Register(MCR) to the corresponding bit value in 'tcrTlrBitVal' 
 *         passed as a parameter to this API.
 *
 *   baseAdd       Memory address of the UART instance being used.
 *   tcrTlrBitVal  The 6th bit in this 32-bit value shall hold a value
 *                       to which the TCRTLR bit(MCR[6]) of MCR has to be
 *                       restored to.
 *
 * return None.
 */

void 
UARTTCRTLRBitValRestore(unsigned int baseAdd,
			unsigned int tcrTlrBitVal)
{
    unsigned int enhanFnBitVal = 0;
    unsigned int lcrRegValue = 0;

    /* Switching to Register Configuration Mode B. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Collecting the current value of EFR[4] and later setting it. */
    enhanFnBitVal = HWREG(baseAdd + UART_EFR) & UART_EFR_ENHANCED_EN;
    HWREG(baseAdd + UART_EFR) |= UART_EFR_ENHANCED_EN;

    /* Switching to Configuration Mode A of operation. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_A);

    /* Programming MCR[6] with the corresponding bit value in 'tcrTlrBitVal'. */
    HWREG(baseAdd + UART_MCR) &= ~(UART_MCR_TCR_TLR);
    HWREG(baseAdd + UART_MCR) |= (tcrTlrBitVal & UART_MCR_TCR_TLR);

    /* Switching to Register Configuration Mode B. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Restoring the value of EFR[4] to its original value. */
    HWREG(baseAdd + UART_EFR) &= ~(UART_EFR_ENHANCED_EN);
    HWREG(baseAdd + UART_EFR) |= enhanFnBitVal;

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;
}

/**
 *    This API enables the specified interrupts in the UART mode of
 *          operation.
 *
 *    baseAdd   Memory address of the UART instance being used.
 *    intFlag   Bit mask value of the bits corresponding to Interrupt
 *                    Enable Register(IER). This specifies the UART interrupts
 *                    to be enabled.
 *
 *  'intFlag' can take one or a combination of the following macros:
 *  - UART_INT_CTS - to enable Clear-To-Send interrupt,
 *  - UART_INT_RTS - to enable Request-To-Send interrupt,
 *  - UART_INT_XOFF - to enable XOFF interrupt,
 *  - UART_INT_SLEEPMODE - to enable Sleep Mode,
 *  - UART_INT_MODEM_STAT - to enable Modem Status interrupt,
 *  - UART_INT_LINE_STAT - to enable Line Status interrupt,
 *  - UART_INT_THR - to enable Transmitter Holding Register Empty interrupt,
 *  - UART_INT_RHR_CTI - to enable Receiver Data available interrupt and
 *                       Character timeout indication interrupt.
 * 
 * return  None.
 *
 * ote    This API modifies the contents of UART Interrupt Enable Register
 *          (IER). Modifying the bits IER[7:4] requires that EFR[4] be set.
 *          This API does the needful before it accesses IER.
 *          Moreover, this API should be called when UART is operating in
 *          UART 16x Mode, UART 13x Mode or UART 16x Auto-baud mode.
 *
 */

void 
UARTIntEnable(unsigned int baseAdd, unsigned int intFlag)
{
    unsigned int enhanFnBitVal = 0;
    unsigned int lcrRegValue = 0;

    /* Switching to Register Configuration Mode B. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Collecting the current value of EFR[4] and later setting it. */
    enhanFnBitVal = HWREG(baseAdd + UART_EFR) & UART_EFR_ENHANCED_EN;
    HWREG(baseAdd + UART_EFR) |= UART_EFR_ENHANCED_EN;

    /* Switching to Register Operational Mode of operation. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    /*
    ** It is suggested that the System Interrupts for UART in the
    ** Interrupt Controller are enabled after enabling the peripheral
    ** interrupts of the UART using this API. If done otherwise, there
    ** is a risk of LCR value not getting restored and illicit characters
    ** transmitted or received from/to the UART. The situation is explained
    ** below.
    ** The scene is that the system interrupt for UART is already enabled and
    ** the current API is invoked. On enabling the interrupts corresponding
    ** to IER[7:4] bits below, if any of those interrupt conditions
    ** already existed, there is a possibility that the control goes to
    ** Interrupt Service Routine (ISR) without executing the remaining
    ** statements in this API. Executing the remaining statements is
    ** critical in that the LCR value is restored in them.
    ** However, there seems to be no risk in this API for enabling interrupts
    ** corresponding to IER[3:0] because it is done at the end and no
    ** statements follow that.
    */

    /************* ATOMIC STATEMENTS START *************************/

    /* Programming the bits IER[7:4]. */
    HWREG(baseAdd + UART_IER) |= (intFlag & 0xF0);

    /* Switching to Register Configuration Mode B. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Restoring the value of EFR[4] to its original value. */
    HWREG(baseAdd + UART_EFR) &= ~(UART_EFR_ENHANCED_EN);
    HWREG(baseAdd + UART_EFR) |= enhanFnBitVal;

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    /************** ATOMIC STATEMENTS END *************************/

    /* Programming the bits IER[3:0]. */
    HWREG(baseAdd + UART_IER) |= (intFlag & 0x0F);
}

/**
 *    This API disables the specified interrupts in the UART mode of
 *          operation.
 *
 *    baseAdd   Memory address of the UART instance being used.
 *    intFlag   Bit mask value of the bits corresponding to Interrupt
 *                    Enable Register(IER). This specifies the UART interrupts
 *                    to be disabled.
 *
 *  'intFlag' can take one or a combination of the following macros:
 *  - UART_INT_CTS - to disable Clear-To-Send interrupt,
 *  - UART_INT_RTS - to disable Request-To-Send interrupt,
 *  - UART_INT_XOFF - to disable XOFF interrupt,
 *  - UART_INT_SLEEPMODE - to disable Sleep Mode,
 *  - UART_INT_MODEM_STAT - to disable Modem Status interrupt,
 *  - UART_INT_LINE_STAT - to disable Line Status interrupt,
 *  - UART_INT_THR - to disable Transmitter Holding Register Empty interrupt,
 *  - UART_INT_RHR_CTI - to disable Receiver Data available interrupt and
 *                       Character timeout indication interrupt.
 *
 * return  None
 *
 * ote  The note section of UARTIntEnable() also applies to this API.
 */

void 
UARTIntDisable(unsigned int baseAdd, unsigned int intFlag)
{
    unsigned int enhanFnBitVal = 0;
    unsigned int lcrRegValue = 0;

    /* Switching to Register Configuration Mode B. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Collecting the current value of EFR[4] and later setting it. */
    enhanFnBitVal = HWREG(baseAdd + UART_EFR) & UART_EFR_ENHANCED_EN;
    HWREG(baseAdd + UART_EFR) |= UART_EFR_ENHANCED_EN;

    /* Switching to Register Operational Mode of operation. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    HWREG(baseAdd + UART_IER) &= ~(intFlag & 0xFF);

    /* Switching to Register Configuration Mode B. */
    UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Restoring the value of EFR[4] to its original value. */
    HWREG(baseAdd + UART_EFR) &= ~(UART_EFR_ENHANCED_EN);
    HWREG(baseAdd + UART_EFR) |= enhanFnBitVal;

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;
}

/**
 *   This API checks whether the TX FIFO (or THR in non-FIFO mode)
 *         is empty or not.
 *
 *   baseAdd    Memory address of the UART instance being used.
 *
 * return true - if the Transmitter FIFO(or THR in non-FIFO mode) is empty.
 *         false - if the Transmitter FIFO(or THR in non-FIFO mode) is
 *         not empty.
 */

unsigned int 
UARTSpaceAvail(unsigned int baseAdd)
{
    unsigned int lcrRegValue = 0;
    unsigned int retVal = false;

    /* Switching to Register Operational Mode of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    /*
    ** Checking if either TXFIFOE or TXSRE bits of Line Status Register(LSR)
    ** are set. TXFIFOE bit is set if TX FIFO(or THR in non-FIFO mode) is
    ** empty. TXSRE is set if both the TX FIFO(or THR in non-FIFO mode) and
    ** the transmitter shift register are empty.
    */

    if(HWREG(baseAdd + UART_LSR) & (UART_LSR_TX_SR_E | UART_LSR_TX_FIFO_E))
    {
        retVal = true;
    }

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return retVal;
}

/**
 *   This API checks if the RX FIFO (or RHR in non-FIFO mode) has atleast
 *         one byte of data to be read.
 * 
 *   baseAdd    Memory address of the UART instance being used.
 *
 * return  true - if there is atleast one data byte present in the RX FIFO
 *          (or RHR in non-FIFO mode)
 *          false - if there are no data bytes present in the RX FIFO(or RHR
 *           in non-FIFO mode)
 */

unsigned int 
UARTCharsAvail(unsigned int baseAdd)
{
    unsigned int lcrRegValue = 0;
    unsigned int retVal = false;

    /* Switching to Register Operational Mode of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    /* Checking if the RHR(or RX FIFO) has atleast one byte to be read. */
    if(HWREG(baseAdd + UART_LSR) & UART_LSR_RX_FIFO_E)
    {
        retVal = true;
    }

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return retVal;
}

/**
 *     This API attempts to write a byte into Transmitter Holding
 *           Register (THR). It checks only once if the transmitter is empty.
 *
 *     baseAdd     Memory address of the UART instance being used.
 *     byteWrite   Byte to be written into the THR register.
 *
 * return   true if the transmitter FIFO(or THR register in non-FIFO mode)
 *           was empty and the character was written. Else it returns false.
 */

unsigned int 
UARTCharPutNonBlocking(unsigned int baseAdd,
		       unsigned char byteWrite)
{
    unsigned int lcrRegValue = 0;
    unsigned int retVal = false;

    /* Switching to Register Operational Mode of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    /*
    ** Checking if either THR alone or both THR and Transmitter Shift Register
    ** are empty.
    */
    if(HWREG(baseAdd + UART_LSR) & (UART_LSR_TX_SR_E | UART_LSR_TX_FIFO_E))
    {
        HWREG(baseAdd + UART_THR) = byteWrite;
        retVal = true;
    }

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;
    
    return retVal;
}


/**
 *     This API reads a byte from the Receiver Buffer Register
 *           (RBR). It checks once if any character is ready to be read.
 *
 *     baseAdd     Memory address of the UART instance being used.
 *
 * return   If the RX FIFO(or RHR) was found to have atleast one byte of
 *           data, then this API reads and returns that byte. Else it
 *           returns -1.
 */

signed char 
UARTCharGetNonBlocking(unsigned int baseAdd)
{
    unsigned int lcrRegValue = 0;
    signed char retVal = -1;

    /* Switching to Register Operational Mode of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    /* Checking if the RX FIFO(or RHR) has atleast one byte of data. */
    if(HWREG(baseAdd + UART_LSR) & UART_LSR_RX_FIFO_E)
    {
        retVal = (signed char)HWREG(baseAdd + UART_RHR);
    }

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return retVal;
}

/**
 *     This API waits indefinitely for the arrival of a byte in
 *           the receiver FIFO. Once a byte has arrived, it returns that
 *           byte.
 *
 *     baseAdd     Memory address of the UART instance being used.
 *
 * return   This returns the read byte.
 */

signed char 
UARTCharGet(unsigned int baseAdd)
{
    unsigned int lcrRegValue = 0;
    signed char retVal = 0;

    /* Switching to Register Operational Mode of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    /* Waits indefinitely until a byte arrives in the RX FIFO(or RHR). */
    while(0 == (HWREG(baseAdd + UART_LSR) & UART_LSR_RX_FIFO_E));

    retVal = ((signed char)HWREG(baseAdd + UART_RHR));

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return retVal;
}

/**
 *    This API waits indefinitely until the Transmitter FIFO
 *          (THR register in non-FIFO mode) and Transmitter Shift
 *          Register are empty. On empty, it writes a byte to the THR.
 *
 *    baseAdd     Memory address of the UART instance being used
 *    byteTx      The byte to be transmitted by the UART.
 *
 * return  None
 */

void 
UARTCharPut(unsigned int baseAdd, unsigned char byteTx)
{
    unsigned int lcrRegValue = 0;

    /* Switching to Register Operational Mode of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    /*
    ** Waits indefinitely until the THR and Transmitter Shift Registers are
    ** empty.
    */
    while((UART_LSR_TX_SR_E | UART_LSR_TX_FIFO_E) !=
          (HWREG(baseAdd + UART_LSR) & (UART_LSR_TX_SR_E | UART_LSR_TX_FIFO_E)));

    HWREG(baseAdd + UART_THR) = byteTx;

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;
}

/**
 *   This API reads the receiver data error status. 
 *
 *   baseAdd     Memory address of the UART instance being used.
 *
 * return This returns the error status. This can be one or a combination 
 *         of the below values:
 *         - UART_OVERRUN_ERROR - indicating Overrun error occured
 *         - UART_PARITY_ERROR - indicating Parity error occured
 *         - UART_FRAMING_ERROR - indicating Framing error occured
 *         - UART_BREAK_DETECTED_ERROR - indicating a Break condition was
 *           detected
 *         - UART_FIFO_PE_FE_BI_DETECTED - indicating that atleast one parity
 *           error, framing error or a break indication is present in the
 *           RX FIFO
 */

unsigned int 
UARTRxErrorGet(unsigned int baseAdd)
{
    unsigned int lcrRegValue = 0;
    unsigned int retVal = 0;

    /* Switching to Register Operational Mode of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    retVal = (HWREG(baseAdd + UART_LSR) & (UART_LSR_RX_FIFO_STS |
                                           UART_LSR_RX_BI |
                                           UART_LSR_RX_FE |
                                           UART_LSR_RX_PE |
                                           UART_LSR_RX_OE));

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return retVal;
}

/**
 *   This API determines the UART Interrupt Status.
 *
 *   baseAdd   Memory address of the UART instance being used.
 * 
 * return This returns one or a combination of the following macros:
 *         - UART_INTID_MODEM_STAT - indicating the occurence of a Modem Status
 *           interrupt
 *         - UART_INTID_TX_THRES_REACH - indicating that the TX FIFO Threshold
 *           number of bytes can be written to the TX FIFO.
 *         - UART_INTID_RX_THRES_REACH - indicating that the RX FIFO has
 *           reached its programmed Trigger Level
 *         - UART_INTID_RX_LINE_STAT_ERROR - indicating the occurence of a
 *           receiver Line Status error
 *         - UART_INTID_CHAR_TIMEOUT - indicating the occurence of a Receiver
 *           Timeout
 *         - UART_INTID_XOFF_SPEC_CHAR_DETECT - indicating the detection of XOFF
 *           or a Special character
 *         - UART_INTID_MODEM_SIG_STATE_CHANGE - indicating that atleast one of
 *           the Modem signals among CTSn, RTSn and DSRn have changed states
 *           from active(low) to inactive(high)
 */

unsigned int 
UARTIntIdentityGet(unsigned int baseAdd)
{
    unsigned int lcrRegValue = 0;
    unsigned int retVal = 0;

    /* Switching to Register Operational Mode of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    retVal = (HWREG(baseAdd + UART_IIR) & UART_IIR_IT_TYPE);

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return retVal;
}

/**
 *   This API determines whether any UART interrupt condition is
 *         still alive and is pending to be serviced.
 * 
 *   baseAdd   Memory address of the UART instance being used.
 *
 * return UART_INT_PENDING - if servicing an interrupt is still pending
 *         UART_N0_INT_PENDING - if there are no interrupts to be serviced
 */

unsigned int 
UARTIntPendingStatusGet(unsigned int baseAdd)
{
    unsigned int retVal = UART_N0_INT_PENDING;
    unsigned int lcrRegValue = 0;

    /* Switching to Register Operational Mode of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    /* Checking if an Interrupt is pending. */
    if(!(HWREG(baseAdd + UART_IIR) & UART_IIR_IT_PENDING))
    {
        retVal = UART_INT_PENDING;
    }

    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return retVal;
}

/**
 *   This API determines whether FIFO mode of operation is enabled
 *         for the UART instance or not.
 *
 *   baseAdd   Memory address of the UART instance being used.
 *
 * return true - if FIFO mode of operation is enabled
 *         false - if FIFO mode of operation is disabled
 */

unsigned int 
UARTFIFOEnableStatusGet(unsigned int baseAdd)
{
    unsigned int lcrRegValue = 0;
    unsigned int retVal = false;

    /* Switching to Register Operational Mode of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_OPERATIONAL_MODE);

    if(HWREG(baseAdd + UART_IIR) & UART_IIR_FCR_MIRROR)
    {
        retVal = true;
    }
    
    /* Restoring the value of LCR. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return retVal;
}

/**
 *   This API controls the use of Auto-RTS and Auto-CTS features which
 *         are used in Hardware Flow Control Mode of operation. The
 *         Auto-RTS and Auto-CTS functions can be individually enabled or
 *         disabled.
 *     
 *   baseAdd         Memory address of the UART instance being used.
 *   autoCtsControl  This specifies whether to enable or disable
 *                         Auto-CTS functionality
 *   autoRtsControl  This specifies whether to enable or disable
 *                         Auto-RTS functionality
 *
 * 'autoCtsControl' can take one of the following values:
 * - UART_AUTO_CTS_DISABLE - to disable Auto-CTS functionality
 * - UART_AUTO_CTS_ENABLE - to enable Auto-CTS functionality
 *
 * 'autoRtsControl' can take either of the following values:
 * - UART_AUTO_RTS_DISABLE - to disable Auto-RTS functionality
 * - UART_AUTO_RTS_ENABLE - to enable Auto-RTS functionality
 *
 * return None.
 *
 * ote   This API switches UART to Configuration Mode B, programs
 *         AUTOCTSEN and AUTORTSEN bits in EFR and reverts the UART back
 *         to the original mode of operation.
 */

void 
UARTAutoRTSAutoCTSControl(unsigned int baseAdd,
			  unsigned int autoCtsControl,
			  unsigned int autoRtsControl)
{
    unsigned int lcrRegValue = 0;     
 
    /* Switching to Configuration Mode B of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);
    
    /* Clearing AUTOCTSEN and AUTORTSEN bits in EFR. */
    HWREG(baseAdd + UART_EFR) &= ~(UART_EFR_AUTO_CTS_EN |
                                   UART_EFR_AUTO_RTS_EN);

    /* Programming AUTOCTSEN and AUTORTSEN bits in EFR. */
    HWREG(baseAdd + UART_EFR) |= (autoCtsControl & UART_EFR_AUTO_CTS_EN);
    HWREG(baseAdd + UART_EFR) |= (autoRtsControl & UART_EFR_AUTO_RTS_EN);

    /* Restoring LCR with the collected value. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;
}

/**
 *   This API controls the feature of detecting a Special Character
 *         arriving in the receiver.
 *
 *    baseAdd     Memory address of the UART instance being used.
 *    controlFlag This specifies whether to enable or disable detection
 *                      of Special Character.
 *
 * 'controlFlag' can take either of the following two values:
 * - UART_SPECIAL_CHAR_DETECT_DISABLE - to disable detection of Special
 *   Character
 * - UART_SPECIAL_CHAR_DETECT_ENABLE - to enable detection of Special
 *   Character
 *
 * return  None.
 *
 * ote   This API switches the UART to Configuration Mode B, programs
 *         SPECIALCHARDETECT field in EFR and reverts the UART back to
 *         the original mode of operation.  
 *
 */

void 
UARTSpecialCharDetectControl(unsigned int baseAdd,
			     unsigned int controlFlag)
{
    unsigned int lcrRegValue = 0;     
 
    /* Switch to Configuration Mode B of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    HWREG(baseAdd + UART_EFR) &= ~(UART_EFR_SPECIAL_CHAR_DETECT);

    /* Programming the SPECIALCHARDETECT bit in EFR. */
    HWREG(baseAdd + UART_EFR) |= (controlFlag & UART_EFR_SPECIAL_CHAR_DETECT);

    /* Restoring LCR with the collected value. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;
}

/**
 *   This API configures the options for Software Flow Control.
 *
 *   baseAdd      Memory address of the UART instance being used.
 *   swFlowCtrl   This specifies one of the options available for
 *                      software flow control.
 *
 * 'swFlowCtrl' can take one of the following values:
 * - UART_NO_SOFTWARE_FLOW_CONTROL - To disable Software Flow control
 * - UART_TX_RX_XON1_XOFF1 - Transmitter transmits XON1, XOFF1;
 *                           Receiver expects XON1, XOFF1
 * - UART_TX_RX_XON2_XOFF2 - Transmitter transmits XON2, XOFF2;
 *                           Receiver expects XON2, XOFF2
 * - UART_TX_RX_XON1_XOFF1_XON2_XOFF2 - Transmitter transmits XON1,XON2,
 *                           XOFF1 and XOFF2; Receiver expects XON1,XON2,
 *                           XOFF1, XOFF2
 *
 * return None.
 * 
 * ote   This API switches the UART to Configuration Mode B, programs
 *         SWFLOWCONTROL field in EFR and reverts the UART back to the
 *         original mode of operation.
 */

void 
UARTSoftwareFlowCtrlOptSet(unsigned int baseAdd,
			   unsigned int swFlowCtrl)
{
    unsigned int lcrRegValue = 0;
 
    /* Switching to Configuration Mode B of operation. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Clearing the SWFLOWCONTROL field in EFR. */
    HWREG(baseAdd + UART_EFR) &= ~(UART_EFR_SW_FLOW_CONTROL);

    /* Configuring the SWFLOWCONTROL field in EFR. */
    HWREG(baseAdd + UART_EFR) |= (swFlowCtrl & UART_EFR_SW_FLOW_CONTROL);

    /* Restoring LCR with the collected value. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;
}


/**
 *   Used only in UART mode, this API is used to control the pulse
 *         shaping feature. Pulse shaping feature could either be enabled
 *         or disabled in UART mode.
 *
 *   baseAdd       Memory address of the UART instance being used.
 *   shapeControl  This specifies whether to enable or disable Pulse
 *                       shaping feature in UART mode
 *
 * 'shapeControl' can take either of the two following values:
 * - UART_PULSE_NORMAL - to disable Pulse Shaping feature which is the Normal
 *   and default configuration
 * - UART_PULSE_SHAPING - to enable Pulse Shaping feature
 *
 * return None
 */

void 
UARTPulseShapingControl(unsigned int baseAdd, unsigned int shapeControl)
{
    /* Clearing the UARTPULSE bit in MDR2. */
    HWREG(baseAdd + UART_MDR2) &= ~(UART_MDR2_UART_PULSE);

    /* Programming the UARTPULSE bit in MDR2. */
    HWREG(baseAdd + UART_MDR2) |= (shapeControl & UART_MDR2_UART_PULSE);
}

/**
 *   This API performs a module reset of the UART module. It also
 *         waits until the reset process is complete.
 *
 *   baseAdd  Memory address of the UART instance being used.
 *
 * return None.
 *
 * ote   This API accesses the System Configuration Register(SYSC) and 
 *         System Status Register(SYSS) to perform module reset and to 
 *         wait until the same is complete.
 */

void 
UARTModuleReset(unsigned int baseAdd)
{
    /* Performing Software Reset of the module. */
    HWREG(baseAdd + UART_SYSC) |= (UART_SYSC_SOFTRESET);

    /* Wait until the process of Module Reset is complete. */
    while(!(HWREG(baseAdd + UART_SYSS) & UART_SYSS_RESETDONE));
}

/**
 *   This API can be used to control the Power Management
 *         request/acknowledgement process.
 *
 *   baseAdd   Memory address of the UART instance being used.
 *   modeFlag  This specifies the Power Management
 *                   request/acknowledgement process to be followed
 *
 * 'modeFlag' can take one of the following values:
 * - UART_IDLEMODE_FORCE_IDLE - to enable Force Idle mode
 * - UART_IDLEMODE_NO_IDLE - to enable No-Idle mode
 * - UART_IDLEMODE_SMART_IDLE - to enable Smart Idle mode
 * - UART_IDLEMODE_SMART_IDLE_WAKEUP - to enable Smart Idle Wakeup mode
 *
 * return None
 */

void 
UARTIdleModeConfigure(unsigned int baseAdd, unsigned int modeFlag)
{
    /* Clearing the IDLEMODE field in SYSC. */
    HWREG(baseAdd + UART_SYSC) &= ~(UART_SYSC_IDLEMODE);

    /* Programming the IDLEMODE field in SYSC. */
    HWREG(baseAdd + UART_SYSC) |= (modeFlag & UART_SYSC_IDLEMODE);
}

/**
 *   This API is used to control(enable/disable) the Wake-Up feature
 *         of the UART.
 *
 *   baseAdd      Memory address of the UART instance being used.
 *   controlFlag  This specifies whether the Wake Up feature should
 *                      be enabled or disabled for the UART instance
 *
 * 'controlFlag' can take one of the following two values:
 * - UART_WAKEUP_ENABLE - to enable Wake-Up feature
 * - UART_WAKEUP_DISABLE - to disable Wake-Up feature
 *
 * return None
 */

void 
UARTWakeUpControl(unsigned int baseAdd, unsigned int controlFlag)
{
    /* Clearing the ENAWAKEUP bit in SYSC register. */
    HWREG(baseAdd + UART_SYSC) &= ~(UART_SYSC_ENAWAKEUP);

    /* Programming the ENAWAKEUP feature in SYSC register. */
    HWREG(baseAdd + UART_SYSC) |= (controlFlag & UART_SYSC_ENAWAKEUP);
}

/**
 *   This API is used to control(enable/disable) the Auto-Idle mode
 *         of operation of the UART.
 *
 *   baseAdd      Memory address of the UART instance being used.
 *   modeFlag     This specifies whether to enable or disable the
 *                      Auto-Idle mode of the UART
 *
 * 'modeFlag' can take one of the following two values:
 * - UART_AUTO_IDLE_MODE_DISABLE - to disable Auto-Idle mode
 * - UART_AUTO_IDLE_MODE_ENABLE - to enable Auto-Idle mode
 *
 * return None
 */

void 
UARTAutoIdleModeControl(unsigned int baseAdd, unsigned int modeFlag)
{
    /* Clearing the AUTOIDLE bit in SYSC register. */
    HWREG(baseAdd + UART_SYSC) &= ~(UART_SYSC_AUTOIDLE);

    /* Programming the AUTOIDLE bit in SYSC register. */
    HWREG(baseAdd + UART_SYSC) |= (modeFlag & UART_SYSC_AUTOIDLE);
}


/**
 *   This API configures the Receiver FIFO threshold level to
 *         start/stop transmission during Hardware Flow Control. 
 *
 *   baseAdd        Memory address of the UART instance being used.
 *   rtsHaltFlag    The receiver FIFO threshold level on attaining
 *                        which the RTS line is deasserted signalling the
 *                        transmitter of its counterpart to stop trasmitting.
 *   rtsStartFlag   The receiver FIFO threshold level on attaining
 *                        which the RTS line is asserted signalling the
 *                        transmitter of its counterpart to start transmitting.
 *
 * 'rtsHaltFlag' can take one among the following values:
 *  - m, where (0 <= m <= 15).
 *  The HALT trigger level would then be (m *  4).
 * 'rtsStartFlag' can take one among the following values:
 *  - n, where (0 <= n <= 15).
 *  The RESTORE trigger level would then be (n * 4).
 *
 * For Example: If m = 8 and n = 5, then the receiver trigger levels would be:
 * HALT = (8 * 4) = 32, RESTORE = (5 * 4) = 20.
 *
 * return None.
 *
 * ote     Here two things should be taken care of:
 *        1> RX FIFO Threshold Level to Halt Transmission should be greater
 *           than the Threshold level to Start transmission
 *           i.e. TCR[3:0] > TCR[7:4].
 *        2> In FIFO Interrupt mode with Flow Control, the RX FIFO threshold
 *           level to Halt Transmission (TCR[3:0]) should be greater than or
 *           equal to the Receiver FIFO trigger level(TLR[7:4] or FCR[7:6]).
 */

void 
UARTFlowCtrlTrigLvlConfig(unsigned int baseAdd,
			  unsigned int rtsHaltFlag,
			  unsigned int rtsStartFlag)
{
    unsigned tcrValue = 0;

    tcrValue = rtsHaltFlag & UART_TCR_RX_FIFO_TRIG_HALT;
    tcrValue |= (rtsStartFlag <<
                 UART_TCR_RX_FIFO_TRIG_START_SHIFT) &
                 UART_TCR_RX_FIFO_TRIG_START;

    /* Writing to TCR register. */
    HWREG(baseAdd + UART_TCR) = tcrValue;
}

/**
 *   This API programs the XON1/ADDR1 and XOFF1 registers.
 *
 *   baseAdd      Memory address of the UART instance being used.
 *   xon1Value    The XON1 character.
 *   xoff1Value   The XOFF1 character.
 *
 * return None.
 *
 * ote   In UART mode, the character in XON1/ADDR1 register is the XON1
 *         character. In IrDA mode, this is the address ADDR1. 
 */

void 
UARTXON1XOFF1ValProgram(unsigned int baseAdd,
			unsigned char xon1Value,
			unsigned char xoff1Value)
{
    /* Programming the XON1 register. */
    HWREG(baseAdd + UART_XON1_ADDR1) = xon1Value;
        
    /* Programming the XOFF1 register. */
    HWREG(baseAdd + UART_XOFF1) = xoff1Value;
}

/**
 *   This API programs the XON2/ADDR2 and XOFF2 registers.
 * 
 *   baseAdd      Memory address of the UART instance being used.
 *   xon2Value    The XON2 character.
 *   xoff2Value   The XOFF2 character.
 *
 * return None.
 *  
 * ote   In UART mode, the character in XON2/ADDR2 register is the XON2
 *         character. In IrDA mode, this is the address ADDR2.
 *
 */

void 
UARTXON2XOFF2ValProgram(unsigned int baseAdd,
			unsigned char xon2Value,
			unsigned char xoff2Value)
{
    /* Programming the XON2 register. */
    HWREG(baseAdd + UART_XON2_ADDR2) = xon2Value;

    /* Programming the XOFF2 register. */
    HWREG(baseAdd + UART_XOFF2) = xoff2Value;
}


/**
 *   This API controls(enables/disables) the XON-any feature in Modem
 *         Control Register(MCR).
 *
 *   baseAdd        Memory address of the UART instance being used.
 *   controlFlag    This specifies whether to enable or disable XON any
 *                        feature
 *
 * 'xonAnyControl' can take one of the following values:
 * - UART_XON_ANY_ENABLE - to enable XON any functionality
 * - UART_XON_ANY_DISABLE - to disable XON any functionality
 *
 * return None.
 *
 * ote   When XON-any feature is enabled, the transmission will resume after
 *         receiving any character after recognizing the XOFF character. The 
 *         XON-any character is written into the RX FIFO even if it is a
 *         software flow character.
 */

void 
UARTXONAnyFeatureControl(unsigned int baseAdd, unsigned int controlFlag)
{
    unsigned int lcrRegValue = 0;
    unsigned int enhanFnBitVal = 0;

    /* Switching to Register Configuration Mode B. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_B);

    /* Collecting the value of EFR[4] and later setting the same to 1. */
    enhanFnBitVal = (HWREG(baseAdd + UART_EFR) & UART_EFR_ENHANCED_EN);
    HWREG(baseAdd + UART_EFR) |= UART_EFR_ENHANCED_EN;

    /* Clearing the XONEN bit in MCR. */
    HWREG(baseAdd + UART_MCR) &= ~(UART_MCR_XON_EN);

    /* Programming the XONEN bit in MCR. */
    HWREG(baseAdd + UART_MCR) |= (controlFlag & UART_MCR_XON_EN);

    /*
    ** Clearing the bit value of EFR[4] and later restoring it to the
    ** original value.
    */
    HWREG(baseAdd + UART_EFR) &= ~(UART_EFR_ENHANCED_EN);
    HWREG(baseAdd + UART_EFR) |= enhanFnBitVal;

    /* Restoring LCR to the original value. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;
}

/**
 *   This API controls(enables/disables) the Loopback mode of operation
 *         for the UART instance.
 *
 *   baseAdd      Memory address of the UART instance being used
 *   controlFlag  This specifies whether to enable or disable Loopback
 *                      mode of operation
 *
 * 'controlFlag' can take one of the following values:
 * - UART_LOOPBACK_MODE_ENABLE - to enable Loopback mode of operation
 * - UART_LOOPBACK_MODE_DISABLE - to disable Loopback mode and thus resulting
 *                                in switching to Normal operating mode
 *
 * return None
 */

void 
UARTLoopbackModeControl(unsigned int baseAdd, unsigned int controlFlag)
{
    /* Clearing the LOOPBACKEN bit in MCR. */
    HWREG(baseAdd + UART_MCR) &= ~(UART_MCR_LOOPBACK_EN);

    /* Programming the LOOPBACKEN bit in MCR. */
    HWREG(baseAdd + UART_MCR) |= (controlFlag & UART_MCR_LOOPBACK_EN);
}

/**
 *   This API switches the specified Modem Control Signals to active
 *         state. The Modem Control signals in context are DCD, RI, RTS and
 *         DTR.
 * 
 *   baseAdd    Memory address of the UART instance being used. 
 *   modeFlag   This specifies the signals that are required to be
 *                    switched to active state. Bits MCR[3:0] hold control
 *                    for switching Modem Control Signals to active/inactive
 *                    state.
 *
 * 'modeFlag' can take one or a combination of the following values:
 * - UART_DCD_CONTROL - specifying to force DCDn input to active state(low)
 * - UART_RI_CONTROL - specifying to force RIn input to active state(low)
 * - UART_RTS_CONTROL - specifying to force RTSn output to active state (low)
 * - UART_DTR_CONTROL - specifying to force DTRn output to active state (low)
 *
 * return None.
 */

void 
UARTModemControlSet(unsigned int baseAdd, unsigned int modeFlag)
{
    /* Programming the specified bits of MCR. */
    HWREG(baseAdd + UART_MCR) |= (modeFlag & (UART_MCR_CD_STS_CH |
                                              UART_MCR_RI_STS_CH |
                                              UART_MCR_RTS |
                                              UART_MCR_DTR));
 
}

/**
 *   This API switches the specified Modem Control signals to inactive
 *         state. The Modem Control signals in context are DCD, RI, RTS and
 *         DTR.
 *
 *   baseAdd    Memory address of the UART instance being used.
 *   modeFlag   This specifies the signals that are required to be
 *                    switched to inactive state. Bits MCR[3:0] hold control
 *                    for switching Modem Control Signals to active/inactive
 *                    state.

 *
 * 'modeFlag' can take one or a combination of the following values:
 * - UART_DCD_CONTROL - specifying to force DCDn input to inactive state(high)
 * - UART_RI_CONTROL - specifying to force RIn input to inactive state (high)
 * - UART_RTS_CONTROL - specifying to force RTSn output to inactive state(high)
 * - UART_DTR_CONTROL - specifying to force DTRn output to inactive state(high)
 * 
 * return None
 */

void 
UARTModemControlClear(unsigned int baseAdd, unsigned int modeFlag)
{
    /* Clearing the specified bits in MCR. */
    HWREG(baseAdd + UART_MCR) &= ~(modeFlag & (UART_MCR_CD_STS_CH |
                                               UART_MCR_RI_STS_CH |
                                               UART_MCR_RTS |
                                               UART_MCR_DTR));
}

/**
 *   This API reads the values on Modem Signal Lines. The Modem Signals
 *         in context are:
 *         1> Data Carrier Detect(DCD)
 *         2> Ring Indicator(RI)
 *         3> Data Set Ready(DSR)
 *         4> Clear To Send(CTS)
 *
 *   baseAdd    Memory address of the UART instance being used.
 *
 * return The value returned could be one of the following macros, a
 *         combination or all of it.
 *         - UART_DCD_VALUE - indicating DCDn line is active(low)
 *         - UART_RI_VALUE - indicating RIn line is active(low)
 *         - UART_DSR_VALUE - indicating DSRn line is active(low)
 *         - UART_CTS_VALUE - indicating CTSn line is active(low)
 */

unsigned int 
UARTModemStatusGet(unsigned int baseAdd)
{
    /*
    ** Reading MSR[7:4] bits. These values indicate the complement of the
    ** signal levels on the Modem input lines.
    */

    return(HWREG(baseAdd + UART_MSR) & (UART_MSR_NCD_STS |
                                        UART_MSR_NRI_STS |
                                        UART_MSR_NDSR_STS |
                                        UART_MSR_NCTS_STS));
}

/**
 *   This API determines if the values on Modem Signal Lines have
 *         changed since the last read of Modem Status Register(MSR).
 *         The Modem Signals in context are DCD, RI, DSR, CTS.
 *
 *   baseAdd    Memory address of the UART instance being used.
 *
 * return The value returned could be one of the following macros, a
 *         combination or all of it.
 *         - UART_DCD_STS_CHANGED - indicating that DCDn input changed state
 *           since the last read of MSR
 *         - UART_RI_STS_CHANGED - indicating that RIn input changed state
 *           since the last read of MSR
 *         - UART_DSR_STS_CHANGED - indicating that DSRn input changed state
 *           since the last read of MSR
 *         - UART_CTS_STS_CHANGED - indicating that CTSn input changed state
 *           since the last read of MSR
 */

unsigned int 
UARTModemStatusChangeCheck(unsigned int baseAdd)
{
    /*
    ** Reading MSR[3:0] bits that indicate the change of state of Modem signal
    ** lines.
    */
    return(HWREG(baseAdd + UART_MSR) & (UART_MSR_DCD_STS |
                                        UART_MSR_RI_STS |
                                        UART_MSR_DSR_STS |
                                        UART_MSR_CTS_STS));
}

/**
 *   This API reads the RESUME register which clears the internal flags.
 * 
 *   baseAdd    Memory address of the UART instance being used
 *
 * return None
 *   
 * ote   When conditions like TX Underrun/RX Overrun errors occur, the
 *         transmission/reception gets halted and some internal flags are set.
 *         Clearing these flags would resume the halted operation.
 */

void 
UARTResumeOperation(unsigned int baseAdd)
{
    /* Dummy read of RESUME register. */
    HWREG(baseAdd + UART_RESUME);
}


/**
 *   This API enables the Wake-Up capability for the specified events.
 *         On enabling Wake-Up capability for them, the occurence of the
 *         corresponding event shall wake up the system.
 *
 *   baseAdd     Memory address of the UART instance being used.
 *   wakeUpFlag  This specifies the event(s) for which Wake-Up
 *                     capability needs to be enabled.
 *
 * 'wakeUpFlag' can take one or a combination of the following values:
 * - UART_WAKEUP_TX_INTERRUPT - enabling THR, TXDMA and TXSTATUS
 *   interrupts to wake up the system
 * - UART_WAKEUP_RLS_INTERRUPT - enabling the Receiver Line Status
 *   interrupt to wake up the system
 * - UART_WAKEUP_RHR_INTERRUPT - enabling the RHR interrupt(RX FIFO
 *   threshold level reached) to wake up the system
 * - UART_WAKEUP_RX_ACTIVITY - enabling any activity on the Receiver line
 *   (RX) to wake up the system
 * - UART_WAKEUP_DCD_ACTIVITY - enabling any activity on DCD line to wake
 *   up the system
 * - UART_WAKEUP_RI_ACTIVITY - enabling any activity on RI line to wake up
 *   the system
 * - UART_WAKEUP_DSR_ACTIVITY - enabling any acivity on DSR line to wake up
 *   the system
 * - UART_WAKEUP_CTS_ACTIVITY - enabling any activity on CTS line to wake up
 *   the system
 *
 * return  None
 */

void 
UARTWakeUpEventsEnable(unsigned int baseAdd, unsigned int wakeUpFlag)
{
    /* Programming the Wake-Up configuration fields in WER. */
    HWREG(baseAdd + UART_WER) |= (wakeUpFlag &
                                  (UART_WER_EVENT_7_TX_WAKEUP_EN |
                                   UART_WER_EVENT_6_RECEIVER_LINE_STATUS_INTERRUPT |
                                   UART_WER_EVENT_5_RHR_INTERRUPT |
                                   UART_WER_EVENT_4_RX_ACTIVITY |
                                   UART_WER_EVENT_3_DCD_CD_ACTIVITY |
                                   UART_WER_EVENT_2_RI_ACTIVITY |
                                   UART_WER_EVENT_1_DSR_ACTIVITY |
                                   UART_WER_EVENT_0_CTS_ACTIVITY));
}

/**
 *   This API disables the Wake-Up capability for the specified events.
 *         On disabling Wake-Up capability for them, the occurence of the
 *         corresponding event shall not wake up the system.
 *
 *   baseAdd     Memory address of the UART instance being used.
 *   wakeUpFlag  This specifies the event(s) for which Wake-Up
 *                     capability needs to be disabled.
 *
 * 'wakeUpFlag' can take one or a combination of the following values:
 * - UART_WAKEUP_TX_INTERRUPT - disabling THR, TXDMA and TXSTATUS
 *   interrupts to wake up the system
 * - UART_WAKEUP_RLS_INTERRUPT - disabling the Receiver Line Status
 *   interrupt to wake up the system
 * - UART_WAKEUP_RHR_INTERRUPT - disabling the RHR interrupt(RX FIFO
 *   threshold level reached) to wake up the system
 * - UART_WAKEUP_RX_ACTIVITY - disabling any activity on the Receiver line
 *   (RX) to wake up the system
 * - UART_WAKEUP_DCD_ACTIVITY - disabling any activity on DCD line to wake
 *   up the system
 * - UART_WAKEUP_RI_ACTIVITY - disabling any activity on RI line to wake up
 *   the system
 * - UART_WAKEUP_DSR_ACTIVITY - disabling any acivity on DSR line to wake up
 *   the system
 * - UART_WAKEUP_CTS_ACTIVITY - disabling any activity on CTS line to wake up
 *   the system
 *
 * return  None
 */

void 
UARTWakeUpEventsDisable(unsigned int baseAdd, unsigned int wakeUpFlag)
{
    /* Programming the Wake-Up configuration fields in WER. */
    HWREG(baseAdd + UART_WER) &= ~(wakeUpFlag &
                                   (UART_WER_EVENT_7_TX_WAKEUP_EN |
                                    UART_WER_EVENT_6_RECEIVER_LINE_STATUS_INTERRUPT |
                                    UART_WER_EVENT_5_RHR_INTERRUPT |
                                    UART_WER_EVENT_4_RX_ACTIVITY |
                                    UART_WER_EVENT_3_DCD_CD_ACTIVITY |
                                    UART_WER_EVENT_2_RI_ACTIVITY |
                                    UART_WER_EVENT_1_DSR_ACTIVITY |
                                    UART_WER_EVENT_0_CTS_ACTIVITY));
}


/**
 *   This API controls the feature of setting the Trigger Level
 *         granularity as 1 for Transmitter and Receiver FIFOs.
 *
 *   baseAdd          Memory address of the UART instance being used.
 *   rxFIFOGranCtrl   This specifies whether the trigger level
 *                          granularity for the RX FIFO is to be 1 or not
 *   txFIFOGranCtrl   This specifies whether the trigger level
 *                          granularity for the TX FIFO is to be 1 or not
 *
 * 'rxFIFOGranCtrl' can take either of the following values:
 * - UART_RX_TRIG_LVL_GRAN_1_DISABLE - to disable usage of a granularity of 1
 *   for RX FIFO Trigger level
 * - UART_RX_TRIG_LVL_GRAN_1_ENABLE - to set a granularity of 1 for RX FIFO
 *   Trigger level
 *
 * 'txFIFOGranCtrl' can take either of the following values:
 * - UART_TX_TRIG_LVL_GRAN_1_DISABLE - to disable usage of a granularity of 1
 *   for TX FIFO Trigger level
 * - UART_TX_FIFO_LVL_GRAN_1_ENABLE - to set a granularity of 1 for TX FIFO
 *   Trigger level
 *
 * return None
 */

void 
UARTFIFOTrigLvlGranControl(unsigned int baseAdd,
			   unsigned int rxFIFOGranCtrl,
			   unsigned int txFIFOGranCtrl)
{
    /* Clearing the RXTRIGGRANU1 and TXTRIGGRANU1 bits in SCR. */
    HWREG(baseAdd + UART_SCR) &= ~(UART_SCR_RX_TRIG_GRANU1 |
                                   UART_SCR_TX_TRIG_GRANU1);

    /* Programming the RXTRIGGRANU1 bit in SCR. */
    HWREG(baseAdd + UART_SCR) |= (rxFIFOGranCtrl & UART_SCR_RX_TRIG_GRANU1);

    /* Programming the TXTRIGGRANU1 bit in SCR. */
    HWREG(baseAdd + UART_SCR) |= (txFIFOGranCtrl & UART_SCR_TX_TRIG_GRANU1);
}

/**
 *   This API controls the interrupt enable and disable feature for
 *         Data Set Ready(DSRn) interrupt.
 *
 *   baseAdd     Memory address of the UART instance being used
 *   controlFlag This specifies whether to enable or disable DSRn
 *                     interrupt
 *
 * 'controlFlag' can take one of the following values:
 * - UART_DSRn_INT_DISABLE - to disable DSRn interrupt
 * - UART_DSRn_INT_ENABLE - to enable DSRn interrupt
 *
 * return None
 */

void 
UARTDSRInterruptControl(unsigned int baseAdd, unsigned int controlFlag)
{
    HWREG(baseAdd + UART_SCR) &= ~(UART_SCR_DSR_IT);
    /* Programming the DSRIT bit in SCR. */
    HWREG(baseAdd + UART_SCR) |= (controlFlag & UART_SCR_DSR_IT);
}

/**
 *   This API is used to choose a condition under which a  
 *         Transmit Holding Register(THR) Interrupt should occur.
 *         A THR interrupt can be configured to occur either when:
 *         1> TX FIFO becoming empty OR
 *         2> TX FIFO and TX Shift register becoming empty.
 *
 *   baseAdd      Memory address of the UART instance being used
 *   controlFlag  This specifies the condition under which a Transmitter
 *                      Holding Register Interrupt should occur.
 *
 * 'controlFlag' can take either of the following two values:
 * - UART_THR_INT_NORMAL - for THR Interrupt to be raised under normal
 *   conditions(guided by the TX FIFO Threshold value)
 * - UART_THR_INT_FIFO_TSR_EMPTY - for THR Interrupt to be raised when both
 *   Transmitter FIFO and Transmitter Shift Register are empty
 *
 * return None
 */

void 
UARTTxEmptyIntControl(unsigned int baseAdd,
		      unsigned int controlFlag)
{
    /* Clearing the TXEMPTYCTLIT bit in SCR. */
    HWREG(baseAdd + UART_SCR) &= ~(UART_SCR_TX_EMPTY_CTL_IT);

    /* Programming the TXEMPTYCTLIT bit in SCR. */
    HWREG(baseAdd + UART_SCR) |= (controlFlag & UART_SCR_TX_EMPTY_CTL_IT);
}


/**
 *   This API controls (enables/disables) a feature where a falling
 *         edge on the RX, CTSn or DSRs could send a wake-up interrupt to
 *         the CPU.
 *
 *   baseAdd     Memory address of the UART instance being used.
 *   wakeUpFlag  This specifies whether or not a wake-up interrupt
 *                     should be sent to the CPU when a falling edge occurs on
 *                     RX, CTSn or DSRn lines.
 *
 * 'wakeUpFlag' can take one of the following values:
 * - UART_RX_CTS_DSR_WAKEUP_DISABLE - to disable generation of a Wake-Up
 *   interrupt due to occurence of a falling edge on RX, CTSn or DSRn lines.
 * - UART_RX_CTS_DSR_WAKEUP_ENABLE - to enable generation of a Wake-Up
 *   interrupt due to occurence of a falling edge on RX, CTSn, DSRn lines.
 *
 * return None
 */

void 
UARTRXCTSDSRWakeUpConfigure(unsigned int baseAdd, unsigned int wakeUpFlag)
{
    /* Clearing the RXCTSDSRWAKEUPENABLE bit in SCR. */
    HWREG(baseAdd + UART_SCR) &= ~(UART_SCR_RX_CTS_DSR_WAKE_UP_ENABLE);

    /* Programming the RXCTSDSRWAKEUPENABLE bit in SCR. */
    HWREG(baseAdd + UART_SCR) |= (wakeUpFlag & UART_SCR_RX_CTS_DSR_WAKE_UP_ENABLE);
}

/**
 *   This API determines whether a falling edge occured on RX, CTSn or
 *         DSRn lines.
 *
 *   baseAdd     Memory address of the UART instance being used.
 *
 * return This returns one of the following values:
 *         - UART_RX_CTS_DSR_NO_FALL_EDGE - indicating that no falling edge
 *           occured on RX, CTSn and DSRn lines
 *         - UART_RX_CTS_DSR_FALL_EDGE - indicating that a falling edge
 *           occured on RX, CTSn and DSRn lines
 */

unsigned int 
UARTRXCTSDSRTransitionStatusGet(unsigned int baseAdd)
{
    return(HWREG(baseAdd + UART_SSR) & UART_SSR_RX_CTS_DSR_WAKE_UP_STS);
}

/**
 *   This API controls the DMA Counter Reset options.
 *
 *   baseAdd      Memory address of the UART instance being used
 *   controlFlag  This specifies the DMA Counter Reset options
 *
 * 'controlFlag' can take either of the following values:
 * - UART_DMA_CNTR_NO_RESET_FIFO_RESET - indicating that the DMA counter
 *   shall not be reset if the corresponding FIFO is reset
 * - UART_DMA_CNTR_RESET_FIFO_RESET - indicating that the DMA counter shall
 *   be reset if the corresponding FIFO is reset
 *
 * return None
 */

void 
UARTDMACounterResetControl(unsigned int baseAdd, unsigned int controlFlag)
{
    /* Clearing the DMACOUNTERRST bit in SSR. */
    HWREG(baseAdd + UART_SSR) &= ~(UART_SSR_DMA_COUNTER_RST);

    /* Programming the DMACOUNTERRST bit in SSR. */
    HWREG(baseAdd + UART_SSR) |= (controlFlag &  UART_SSR_DMA_COUNTER_RST);
}

/**
 *   This API determines whether the Transmitter FIFO is full or not.
 *
 *   baseAdd      Memory address of the UART instance being used
 *
 * return This returns either of the following values:
 *         - UART_TX_FIFO_NOT_FULL - indicating that the TX FIFO is not full
 *         - UART_TX_FIFO_FULL - indicating that the TX FIFO is full
 */

unsigned int 
UARTTxFIFOFullStatusGet(unsigned int baseAdd)
{
    return (HWREG(baseAdd + UART_SSR) & UART_SSR_TX_FIFO_FULL);
}

/**
 *   This API determines the Parity mode being configured by the system
 *         in the UART Autobauding mode.
 *
 *   baseAdd     Memory address of the UART instance being used.
 *
 * return This returns one of the following values:
 *         - UART_AUTOBAUD_NO_PARITY - indicating that no parity was
 *           identified
 *         - UART_AUTOBAUD_PARITY_SPACE - indicating that space parity has
 *           been configured
 *         - UART_AUTOBAUD_EVEN_PARITY - indicating that even parity has been
 *           configured
 *         - UART_AUTOBAUD_ODD_PARITY - indicating that odd parity has been
 *           configured
 *
 * ote   UASR register used in this API can be accessed only when the UART
 *         is in Configuration Mode A or Configuration Mode B of operation.
 */

unsigned int 
UARTAutobaudParityGet(unsigned int baseAdd)
{
    unsigned int lcrRegValue = 0;
    unsigned int retVal = 0;

    /* Switching to Register Configuration Mode A. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_A);

    retVal = (HWREG(baseAdd + UART_UASR) & UART_UASR_PARITY_TYPE);

    /* Restoring the value of LCR to its original value. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return retVal;
}

/**
 *   This API determines the word length per frame(character length)
 *         being configured by the system in UART Autobauding mode.
 *
 *   baseAdd     Memory address of the UART instance being used.
 *
 * return This returns one of the following two values:
 *         - UART_AUTOBAUD_CHAR_LENGTH_7 - indicating word length of 7 bits
 *         - UART_AUTOBAUD_CHAR_LENGTH_8 - indicating word length of 8 bits
 *
 * ote   UASR register used in this API can be accessed only when the UART
 *         is in Configuration Mode A or Configuration Mode B of operation.
 */

unsigned int 
UARTAutobaudWordLenGet(unsigned int baseAdd)
{
    unsigned int lcrRegValue = 0;
    unsigned int retVal = 0;

    /* Switching to Register Configuration Mode A. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_A);

    retVal = (HWREG(baseAdd + UART_UASR) & UART_UASR_BIT_BY_CHAR);

    /* Restoring the value of LCR to its original value. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return retVal;
}

/**
 *   This API determines the baud rate being configured by the system
 *         in UART Autobauding mode.
 *
 *   baseAdd     Memory address of the UART instance being used.
 *
 * return This returns one of the following values:
 *         - UART_AUTOBAUD_SPEED_115200 - for baud rate of 115200 bps
 *         - UART_AUTOBAUD_SPEED_57600 - for baud rate of 57600 bps
 *         - UART_AUTOBAUD_SPEED_38400 - for baud rate of 38400 bps
 *         - UART_AUTOBAUD_SPEED_28800 - for baud rate of 28800 bps
 *         - UART_AUTOBAUD_SPEED_19200 - for baud rate of 19200 bps
 *         - UART_AUTOBAUD_SPEED_14400 - for baud rate of 14400 bps
 *         - UART_AUTOBAUD_SPEED_9600 - for baud rate of 9600 bps
 *         - UART_AUTOBAUD_SPEED_4800 - for baud rate of 4800 bps
 *         - UART_AUTOBAUD_SPEED_2400 - for baud rate of 2400 bps
 *         - UART_AUTOBAUD_SPEED_1200 - for baud rate of 1200 bps
 *         - UART_AUTOBAUD_NO_SPEED_IDEN - for no speed identified
 *
 * ote   UASR register used in this API can be accessed only when the UART
 *         is in Configuration Mode A or Configuration Mode B of operation.
 */

unsigned int 
UARTAutobaudSpeedGet(unsigned int baseAdd)
{
    unsigned int lcrRegValue = 0;
    unsigned int retVal = 0;

    /* Switching to Register Configuration Mode A. */
    lcrRegValue = UARTRegConfigModeEnable(baseAdd, UART_REG_CONFIG_MODE_A);

    retVal = (HWREG(baseAdd + UART_UASR) & UART_UASR_SPEED);

    /* Restoring the value of LCR to its original value. */
    HWREG(baseAdd + UART_LCR) = lcrRegValue;

    return retVal;
}

/**
 *   This API programs the Scratchpad Register with the specified
 *         value.
 *
 *   baseAdd      Memory address of the UART instance being used
 *   scratchValue This is the scratch value(temporary data) to be loaded
 *                      to the Scratchpad Register
 * 
 * return None
 */

void 
UARTScratchPadRegWrite(unsigned int baseAdd, unsigned int scratchValue)
{
    HWREG(baseAdd + UART_SPR) &= ~(UART_SPR_SPR_WORD);
    /* Programming the SPR_WORD field of SPR. */
    HWREG(baseAdd + UART_SPR) |= (scratchValue & UART_SPR_SPR_WORD);
}

/**
 *   This API reads the value in Scratchpad Register.
 * 
 *   baseAdd      Memory address of the UART instance being used
 *
 * return The value in Scratchpad Register
 */

unsigned int 
UARTScratchPadRegRead(unsigned int baseAdd)
{
    return (HWREG(baseAdd + UART_SPR) & UART_SPR_SPR_WORD);
}

/**
 *   This API reads the Revision Number of the module from the Module
 *         Version Register(MVR).
 *
 *   baseAdd    Memory address of the UART instance being used
 *
 * return This returns the Major Revision Number(MVR[7:4] and Minor Revision
 *         Number(MVR[3:0])) of the module.
 */

unsigned int 
UARTModuleVersionNumberGet(unsigned int baseAdd)
{
    return (HWREG(baseAdd + UART_MVR) & 0x00FF);
}


/**
 *    This function controls the method of setting the Transmit DMA
 *          Threshold Value. The Transmit DMA Threshold Value can be set to
 *          a default value of 64 characters or can take the value in
 *          TX_DMA_THRESHOLD register.
 *
 *    baseAdd      Memory address of the UART instance being used.
 *    thrsCtrlFlag A value which signifies the method of setting the
 *                       Transmit DMA Threshold Value.
 *
 * 'thrsCtrlFlag' can take one of the following values:
 * - UART_TX_DMA_THRESHOLD_64 - for 64 characters TX DMA Threshold value
 * - UART_TX_DMA_THRESHOLD_REG - for Transmit DMA Threshold value to be
 *   the value in TX_DMA_THRESHOLD register.
 *
 * return  None
 *
 */

void 
UARTTxDMAThresholdControl(unsigned int baseAdd, unsigned int thrsCtrlFlag)
{
    /* Clearing the SET_TX_DMA_THRESHOLD bit in MDR3 register. */
    HWREG(baseAdd + UART_MDR3) &= ~(UART_MDR3_SET_DMA_TX_THRESHOLD);

    /* Programming the SET_TX_DMA_THRESHOLD bit in MDR3 register. */
    HWREG(baseAdd + UART_MDR3) |= (thrsCtrlFlag & UART_MDR3_SET_DMA_TX_THRESHOLD );
}


/**
 *    This function programs the TX_DMA_THRESHOLD register which holds
 *          Transmit DMA Threshold value to be used. When a TX DMA Threshold
 *          value other than 64 characters is required, this function can be
 *          used to program the same.
 *
 *    baseAdd     Memory address of the UART instance being used.
 *    thrsValue   The Transmit DMA Threshold Value.
 *
 * 'thrsValue' can take one of the following value - (0 <= thrsValue <= 63).
 *
 * return  None
 *
 */

void 
UARTTxDMAThresholdValConfig(unsigned int baseAdd, unsigned int thrsValue)
{
    /* Clearing the TX_DMA_THRESHOLD field of TX DMA Threshold register. */
    HWREG(baseAdd + UART_TX_DMA_THRESHOLD) &=
        ~(UART_TX_DMA_THRESHOLD_TX_DMA_THRESHOLD);

    /* Programming the TX_DMA_THRESHOLD field of TX DMA Threshold register. */
    HWREG(baseAdd + UART_TX_DMA_THRESHOLD) |=
        (thrsValue & UART_TX_DMA_THRESHOLD_TX_DMA_THRESHOLD);
}

