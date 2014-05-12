#ifndef TIMER_H
#define TIMER_H

#include "xbool.h"
#include "xint.h"

/*
 * Timer control
 */

/* Setup the debug timer */
void dbg_tmr_setup(void);

/* Reset the debug timer. */
void dbg_tmr_reset(void);

/* Get the current value of the debug timer. */
uint32_t dbg_tmr_get(void);

#endif
