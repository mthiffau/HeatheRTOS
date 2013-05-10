#ifdef TIMER_H
#error "double-included timer.h"
#endif

#define TIMER_H

UTIL_H;

/*
 * Timer control
 */

/* Enable or disable the 32-bit timer. */
void tmr32_enable(bool enable);

/* Set the clock for the 32-bit timer. Valid kHz values are 508 and 2. */
int tmr32_set_kHz(int kHz);

/* Enable or disable periodic mode for the 32-bit timer. */
void tmr32_set_periodic(bool periodic);

/* Set the initial value of the timer.
 * This is also the reload value in periodic mode. */
void tmr32_load(uint32_t x);

/* Read the current value of the timer. */
uint32_t tmr32_get(void);
