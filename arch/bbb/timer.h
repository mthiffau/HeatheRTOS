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
void dbg_tmr_reset(void);

/* Get the current value of the 40-bit timer. */
uint32_t dbg_tmr_get(void);

