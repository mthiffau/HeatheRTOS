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
    float a = 0.5;
    float b = (float)wakeup_time;
    if (a < b)
      while(1);

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
