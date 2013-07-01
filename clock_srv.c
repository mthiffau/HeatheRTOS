#include "u_tid.h"
#include "clock_srv.h"

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "pqueue.h"
#include "timer.h"

#include "xassert.h"
#include "u_syscall.h"
#include "u_events.h"
#include "ns.h"
#include "array_size.h"

enum {
    CLKMSG_TICK,
    CLKMSG_TIME,
    CLKMSG_DELAY,
    CLKMSG_DELAYUNTIL
};

struct clkmsg {
    int type;
    int ticks;
};

struct clksrv {
    int ticks;
    tid_t              tids[MAX_TASKS];
    struct pqueue      delays; /* TIDs' low bytes keyed on wakeup time */
    struct pqueue_node delay_nodes[MAX_TASKS];
};

static void clksrv_init(struct clksrv *clk);
static void clksrv_notify(void);
static void clksrv_cleanup(void);
static int  clksrv_notify_cb(void*, size_t);
static void clksrv_delayuntil(struct clksrv *clk, tid_t who, int ticks);
static void clksrv_undelay(struct clksrv *clk);

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

    tmr32_intr_clear();
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
            assertv(rc, rc == 0);
            clk.ticks++;
            clksrv_undelay(&clk);
            break;
        case CLKMSG_TIME:
            rply = clk.ticks;
            rc = Reply(client, &rply, sizeof (rply));
            assertv(rc, rc == 0);
            break;
        case CLKMSG_DELAY:
            clksrv_delayuntil(&clk, client, clk.ticks + msg.ticks);
            break;
        case CLKMSG_DELAYUNTIL:
            clksrv_delayuntil(&clk, client, msg.ticks);
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
    pqueue_init(&clk->delays, ARRAY_SIZE(clk->delay_nodes), clk->delay_nodes);
    rc = Create(PRIORITY_MAX, &clksrv_notify);
    assertv(rc, rc >= 0);
}

static void
clksrv_notify(void)
{
    tid_t         clksrv_tid;
    struct clkmsg msg;
    int rc;

    clksrv_cleanup();
    RegisterCleanup(&clksrv_cleanup);

    rc = clock_init(100);
    assertv(rc, rc == 0);

    rc = RegisterEvent(IRQ_CLOCK_TICK, &clksrv_notify_cb);
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

static void
clksrv_cleanup(void)
{
    /* TODO? disable 40-bit timer as well */
    tmr32_enable(false);
}

static int
clksrv_notify_cb(void *ptr, size_t n)
{
    assertv(ptr, ptr == NULL);
    assertv(n,   n   == 0);
    tmr32_intr_clear();
    return 0;
}

static void
clksrv_delayuntil(struct clksrv *clk, tid_t who, int when_ticks)
{
    if (when_ticks > clk->ticks) {
        /* Add to priority queue */
        int rc;
        clk->tids[who & 0xff] = who;
        rc = pqueue_add(&clk->delays, who & 0xff, when_ticks);
        assertv(rc, rc == 0); /* we should always have enough space */
    } else {
        /* Reply immediately */
        int rc, rply;
        rply = when_ticks == clk->ticks ? CLOCK_OK : CLOCK_DELAY_PAST;
        rc = Reply(who, &rply, sizeof (rply));
        assertv(rc, rc == 0);
    }
}

static void
clksrv_undelay(struct clksrv *clk)
{
    struct pqueue_entry *delay;
    while ((delay = pqueue_peekmin(&clk->delays)) != NULL) {
        int rc, rply;
        if (delay->key > clk->ticks)
            break; /* no more tasks ready to wake up; all times in future */
        rply = CLOCK_OK;
        rc = Reply(clk->tids[delay->val], &rply, sizeof (rply));
        assertv(rc, rc == 0);
        pqueue_popmin(&clk->delays);
    }
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
