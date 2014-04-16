#include "hw_control_AM335x.h"
#include "soc_AM335x.h"
#include "hw_cm_wkup.h"
#include "hw_cm_per.h"
#include "beaglebone.h"
#include "hw_types.h"

/**
 * This function selects the UART pins for use. The UART pins
 * are multiplexed with pins of other peripherals in the SoC
 *          
 * instanceNum       The instance number of the UART to be used.
 *
 * return  None.
 *
 * This pin multiplexing depends on the profile in which the EVM
 * is configured.
 */
void UARTPinMuxSetup(unsigned int instanceNum)
{
     if(0 == instanceNum)
     {
          /* RXD */
          HWREG(SOC_CONTROL_REGS + CONTROL_CONF_UART_RXD(0)) = 
          (CONTROL_CONF_UART0_RXD_CONF_UART0_RXD_PUTYPESEL | 
           CONTROL_CONF_UART0_RXD_CONF_UART0_RXD_RXACTIVE);

          /* TXD */
          HWREG(SOC_CONTROL_REGS + CONTROL_CONF_UART_TXD(0)) = 
           CONTROL_CONF_UART0_TXD_CONF_UART0_TXD_PUTYPESEL;
     }
}

