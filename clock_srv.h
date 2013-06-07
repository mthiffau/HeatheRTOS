#ifdef CLOCK_SRV_H
#error "double-included clock_srv.h"
#endif

#define CLOCK_SRV_H

XBOOL_H;
XINT_H;

#define HWCLOCK_Hz  508469

struct clock;

/* Initialize the clock so that it ticks at freq_Hz.
 * This invalidates any previously initialized clocks. */
int clock_init(struct clock *clock, int freq_Hz);

/* Update the clock state based on current timer values.
 * Returns true if there was a tick, false otherwise. */
bool clock_update(struct clock *clock);

/* Struct definition */
struct clock {
    uint32_t ticks;   /* number of full clock ticks observed */
    uint32_t _reload; /* clock wraps around to this */
    uint32_t _last;   /* previously read clock value */
};
