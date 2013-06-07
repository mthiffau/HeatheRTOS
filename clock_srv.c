#include "clock_srv.h"

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "timer.h"

#include "xassert.h"
#include "u_tid.h"
#include "u_syscall.h"
#include "u_events.h"

#include "xarg.h"
#include "bwio.h"

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
    int rc;

    clksrv_init(&clk);

    rc = RegisterAs("clock");
    assert(rc == 0);

    for (;;) {
        rc = Receive(&client, &msg, sizeof (msg));
        assert(rc == sizeof (msg));
        switch (msg.type) {
        case CLKMSG_TICK:
            rc = Reply(client, NULL, 0);
            assert(rc == 0);
            clk.ticks++;
            bwprintf(COM2, "tick %d\n", clk.ticks);
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
    assert(rc >= 0);
}

static void
clksrv_notify(void)
{
    tid_t         clksrv_tid;
    struct clkmsg msg;
    int rc;

    rc = clock_init(2);
    assert(rc == 0);

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
    assert(ptr == NULL);
    assert(n   == 0);
    tmr32_intr_clear();
    return 0;
}
