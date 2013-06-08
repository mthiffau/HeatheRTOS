#include "u_tid.h"
#include "clock_srv.h"

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "timer.h"

#include "xassert.h"
#include "u_syscall.h"
#include "u_events.h"
#include "ns.h"

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

struct clksrv {
    int ticks;
};

static void clksrv_init(struct clksrv *clk);
static void clksrv_notify(void);
static int  clksrv_notify_cb(void*, size_t);

int
clock_init(int freq_Hz)
{
    /* Set up timer to produce interrupts at freq_Hz */
    uint32_t reload = (HWCLOCK_Hz + freq_Hz / 2) / freq_Hz - 1; /* rounded */

    /* Set up the 32-bit timer. */
    tmr32_enable(false);
    tmr32_load(reload);
    tmr32_set_periodic(true);
    if (tmr32_set_kHz(HWCLOCK_Hz / 1000) != 0)
        return -1;

    tmr32_enable(true);
    return 0;
}

void
clksrv_main(void)
{
    struct clksrv clk;
    struct clkmsg msg;
    tid_t client;
    int rc, rply;

    clksrv_init(&clk);

    rc = RegisterAs("clock");
    assertv(rc, rc == 0);

    for (;;) {
        rc = Receive(&client, &msg, sizeof (msg));
        assert(rc == sizeof (msg));
        switch (msg.type) {
        case CLKMSG_TICK:
            rc = Reply(client, NULL, 0);
            assert(rc == 0);
            clk.ticks++;
            break;

        case CLKMSG_TIME:
            rply = clk.ticks;
            rc = Reply(client, &rply, sizeof (rply));
            assert(rc == 0);
            break;

        default:
            panic("unrecognized clock server message: %d", msg.type);
        }
    }
}

static void
clksrv_init(struct clksrv *clk)
{
    int rc;
    clk->ticks = 0;
    rc = Create(PRIORITY_MAX, &clksrv_notify);
    assertv(rc, rc >= 0);
}

static void
clksrv_notify(void)
{
    tid_t         clksrv_tid;
    struct clkmsg msg;
    int rc;

    rc = clock_init(2);
    assertv(rc, rc == 0);

    rc = RegisterEvent(EV_CLOCK_TICK, IRQ_CLOCK_TICK, &clksrv_notify_cb);
    assert(rc == 0);

    clksrv_tid = WhoIs("clock");
    for (;;) {
        rc = AwaitEvent(NULL, 0);
        assert(rc == 0);
        msg.type = CLKMSG_TICK;
        rc = Send(clksrv_tid, &msg, sizeof (msg), NULL, 0);
        assert(rc == 0);
    }
}

static int
clksrv_notify_cb(void *ptr, size_t n)
{
    assertv(ptr, ptr == NULL);
    assertv(n,   n   == 0);
    tmr32_intr_clear();
    return 0;
}

void
clkctx_init(struct clkctx *ctx)
{
    ctx->clksrv_tid = WhoIs("clock");
}

int
Time(struct clkctx *ctx)
{
    struct clkmsg msg;
    int rc, rply;
    msg.type = CLKMSG_TIME;
    rc = Send(ctx->clksrv_tid, &msg, sizeof (msg), &rply, sizeof (rply));
    assertv(rc, rc == sizeof (rply));
    return rply;
}

int
Delay(struct clkctx *ctx, int ticks)
{
    struct clkmsg msg;
    int rc, rply;
    msg.type  = CLKMSG_DELAY;
    msg.ticks = ticks;
    rc = Send(ctx->clksrv_tid, &msg, sizeof (msg), &rply, sizeof (rply));
    assertv(rc, rc == sizeof (rply));
    return rply;
}

int
DelayUntil(struct clkctx *ctx, int when_ticks)
{
    struct clkmsg msg;
    int rc, rply;
    msg.type  = CLKMSG_DELAYUNTIL;
    msg.ticks = when_ticks;
    rc = Send(ctx->clksrv_tid, &msg, sizeof (msg), &rply, sizeof (rply));
    assertv(rc, rc == sizeof (rply));
    return rply;
}
