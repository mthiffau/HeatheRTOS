#ifdef CLOCK_H
#error "double-included clock.h"
#endif

#define CLOCK_H

UTIL_H;

#define HWCLOCK_kHz 508
#define HWCLOCK_Hz  (HWCLOCK_kHz * 1000)

struct clock;

/* Initialize the clock so that it ticks at freq_Hz, which must evenly
 * divide HWCLOCK_Hz. This invalidates any previously initialized clocks. */
int clock_init(struct clock *clock, int freq_Hz);

/* Update the clock state based on current timer values.
 * Returns true if there was a tick, false otherwise. */
bool clock_update(struct clock *clock);

/* Get the number of elapsed ticks. */
uint32_t clock_ticks(struct clock *clock);

/* Struct definition */
struct clock {
    uint32_t _ticks;  /* number of full clock ticks observed */
    uint32_t _tickLen;/* the weight assigned to a tick */
    uint32_t _reload; /* clock wraps around to this */
    uint32_t _last;   /* previously read clock value */
};

