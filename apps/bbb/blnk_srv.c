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

    blnksrv_init();

    struct clkctx clock;
    clkctx_init(&clock);

    unsigned int pin_state = GPIO_PIN_HIGH;

    unsigned int minute_counter = 0;

    /* Blink Indefinately */
    while(1){

	/* Delay 0.5 Seconds */
	Delay(&clock, 500);

	pin_state = (pin_state == GPIO_PIN_HIGH ? GPIO_PIN_LOW : GPIO_PIN_HIGH);

	GPIOPinWrite(GPIO_INSTANCE_ADDRESS,
		     GPIO_INSTANCE_PIN_NUMBER,
		     pin_state);

	/* Double blink every minute */
	minute_counter += 1;
	if (minute_counter == 120) {
	    assert(pin_state == GPIO_PIN_HIGH);

	    for(i = 0; i < 3; i++) {
		Delay(&clock, 250);
		pin_state = (pin_state == GPIO_PIN_HIGH ? GPIO_PIN_LOW : GPIO_PIN_HIGH);
		GPIOPinWrite(GPIO_INSTANCE_ADDRESS,
			     GPIO_INSTANCE_PIN_NUMBER,
			     pin_state);
	    }
	    assert(pin_state == GPIO_PIN_LOW);
	    Delay(&clock, 250);

	    minute_counter = 1;
	}
	
    }

    // Shutdown();
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
