#ifdef TIMER_H
#error "double-included timer.h"
#endif

#define TIMER_H

XBOOL_H;
XINT_H;

/*
 * Timer control
 */

/* Setup the debug timer */
void dbg_tmr_setup(void);

/* Reset the debug timer. */
void dbg_tmr_reset(void);

/* Get the current value of the debug timer. */
uint32_t dbg_tmr_get(void);

