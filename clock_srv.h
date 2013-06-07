#ifdef CLOCK_SRV_H
#error "double-included clock_srv.h"
#endif

#define CLOCK_SRV_H

#define HWCLOCK_Hz  508469

enum {
    CLKMSG_TICK,
    CLKMSG_TIME,
    CLKMSG_DELAY,
    CLKMSG_DELAYUNTIL
};

enum {
    CLKRPLY_OK         =  0,
    CLKRPLY_DELAY_PAST = -3
};

struct clkmsg {
    int type;
    int ticks;
};

/* Initialize the clock so that it ticks at freq_Hz.
 * This invalidates any previously initialized clocks. */
int clock_init(int freq_Hz);

/* Clock server entry point. */
void clksrv_main(void);
