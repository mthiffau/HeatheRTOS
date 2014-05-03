#ifdef CLOCK_SRV_H
#error "double-included clock_srv.h"
#endif

#define CLOCK_SRV_H

U_TID_H;

#define CLOCK_OVF (0xFFFFFFFF)
#define CLOCK_1ms (0x5DBF)
#define CLOCK_1us (0x18)

enum {
    CLOCK_OK         =  0,
    CLOCK_DELAY_PAST = -3
};

struct clkctx;

/* Initialize the clock so that it ticks at freq_Hz.
 * This invalidates any previously initialized clocks. */
int clock_init(void);

/* Clock server entry point. */
void clksrv_main(void);

/* Initialize a clock context. This blocks until the clock server starts. */
void clkctx_init(struct clkctx *ctx);

/* Get the current time */
int Time(struct clkctx *ctx);

/* Block for a given number of ticks. */
int Delay(struct clkctx *ctx, int ticks);

/* Block until a given time (in ticks). */
int DelayUntil(struct clkctx *ctx, int when_ticks);

/* Struct body */
struct clkctx {
    tid_t clksrv_tid;
};
