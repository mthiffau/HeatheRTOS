#ifdef TIMER_H
#error "double-included timer.h"
#endif

#define TIMER_H

XBOOL_H;
XINT_H;

/*
 * Timer control
 */

/* Enable and reset the 40-bit timer. */
void tmr40_reset(void);

/* Get the current value of the 40-bit timer. */
uint32_t tmr40_get(void);

/* Enable or disable the 32-bit timer. */
void tmr32_enable(bool enable);

/* Set the clock for the 32-bit timer. Valid kHz values are 508 and 2. */
int tmr32_set_kHz(int kHz);

/* Enable or disable periodic mode for the 32-bit timer. */
void tmr32_set_periodic(bool periodic);

/* Set the initial value of the 32-bit timer.
 * This is also the reload value in periodic mode. */
void tmr32_load(uint32_t x);

/* Read the current value of the 32-bit timer. */
uint32_t tmr32_get(void);
