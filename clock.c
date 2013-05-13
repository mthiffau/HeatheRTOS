#include "xbool.h"
#include "xint.h"
#include "clock.h"

#include "timer.h"

#include "bwio.h"

int clock_init(struct clock *clock, int freq_Hz)
{
    /* Initialize the clock data. */
    clock->ticks   = 0;
    clock->_reload = (HWCLOCK_Hz + freq_Hz / 2) / freq_Hz - 1; /* rounded */
    clock->_last   = clock->_reload + 1; /* impossible value for none */

    /* Set up the 32-bit timer. */
    tmr32_enable(false);
    tmr32_load(clock->_reload);
    tmr32_set_periodic(true);
    if (tmr32_set_kHz(HWCLOCK_Hz / 1000) != 0)
        return -1;

    tmr32_enable(true);
    return 0;
}

bool clock_update(struct clock *clock)
{
    uint32_t now;
    bool     tick;

    /* Check for a tick */
    now  = tmr32_get();
    tick = now > clock->_last;

    /* Update clock data */
    clock->_last = now;
    if (tick)
        clock->ticks++;

    return tick;
}
