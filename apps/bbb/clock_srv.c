#include "u_tid.h"
#include "clock_srv.h"

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "pqueue.h"

#include "xassert.h"
#include "u_syscall.h"
#include "u_events.h"
#include "ns.h"
#include "array_size.h"

#include "soc_AM335x.h"
#include "hw_types.h"
#include "hw_cm_per.h"
#include "hw_cm_dpll.h"
#include "dmtimer.h"

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
    /* Clock ticks in micro seconds */
    int us_ticks;
    /* Clock ticks in milliseconds */
    int ms_ticks;
    /* Number of microsecond ticks since last millisecond tick */
    int intermediate_us;

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

static void 
DMTimer3ModuleClkConfig(void)
{
    /* Select the clock source for the Timer3 instance. */
    HWREG(SOC_CM_DPLL_REGS + CM_DPLL_CLKSEL_TIMER3_CLK) &=
	~(CM_DPLL_CLKSEL_TIMER3_CLK_CLKSEL);

    HWREG(SOC_CM_DPLL_REGS + CM_DPLL_CLKSEL_TIMER3_CLK) |=
	CM_DPLL_CLKSEL_TIMER3_CLK_CLKSEL_CLK_M_OSC;

    while((HWREG(SOC_CM_DPLL_REGS + CM_DPLL_CLKSEL_TIMER3_CLK) &
           CM_DPLL_CLKSEL_TIMER3_CLK_CLKSEL) !=
	  CM_DPLL_CLKSEL_TIMER3_CLK_CLKSEL_CLK_M_OSC);

    HWREG(SOC_CM_PER_REGS + CM_PER_TIMER3_CLKCTRL) |=
	CM_PER_TIMER3_CLKCTRL_MODULEMODE_ENABLE;

    while((HWREG(SOC_CM_PER_REGS + CM_PER_TIMER3_CLKCTRL) &
	   CM_PER_TIMER3_CLKCTRL_MODULEMODE) != CM_PER_TIMER3_CLKCTRL_MODULEMODE_ENABLE);

    while((HWREG(SOC_CM_PER_REGS + CM_PER_TIMER3_CLKCTRL) & 
	   CM_PER_TIMER3_CLKCTRL_IDLEST) != CM_PER_TIMER3_CLKCTRL_IDLEST_FUNC);

    while(!(HWREG(SOC_CM_PER_REGS + CM_PER_L3S_CLKSTCTRL) &
            CM_PER_L3S_CLKSTCTRL_CLKACTIVITY_L3S_GCLK));

    while(!(HWREG(SOC_CM_PER_REGS + CM_PER_L3_CLKSTCTRL) &
            CM_PER_L3_CLKSTCTRL_CLKACTIVITY_L3_GCLK));

    while(!(HWREG(SOC_CM_PER_REGS + CM_PER_OCPWP_L3_CLKSTCTRL) &
	    (CM_PER_OCPWP_L3_CLKSTCTRL_CLKACTIVITY_OCPWP_L3_GCLK |
	     CM_PER_OCPWP_L3_CLKSTCTRL_CLKACTIVITY_OCPWP_L4_GCLK)));

    while(!(HWREG(SOC_CM_PER_REGS + CM_PER_L4LS_CLKSTCTRL) &
	    (CM_PER_L4LS_CLKSTCTRL_CLKACTIVITY_L4LS_GCLK |
	     CM_PER_L4LS_CLKSTCTRL_CLKACTIVITY_TIMER3_GCLK)));
}

int
clock_init()
{
    /* Set up timer to produce interrupts every microsecond */

    /* Configure functional clock */
    DMTimer3ModuleClkConfig();

    /* Disable module */
    DMTimerDisable(SOC_DMTIMER_3_REGS);

    /* Re-load timer counter to zero */
    DMTimerCounterSet(SOC_DMTIMER_3_REGS, CLOCK_RELOAD);

    /* Set re-load value for timer */
    // Period = (0xFFFFFFFF - reload + 1) * clock_rate * divider
    // clock_rate = 24 MHz
    // divider = 8
    DMTimerReloadSet(SOC_DMTIMER_3_REGS, CLOCK_RELOAD);

    /* Make timer auto-reload, compare mode */
    DMTimerModeConfigure(SOC_DMTIMER_3_REGS, DMTIMER_AUTORLD_CMP_ENABLE);

    /* Set pre-scaler value */
    DMTimerPreScalerClkEnable(SOC_DMTIMER_3_REGS, DMTIMER_PRESCALER_CLK_DIV_BY_8);

    /* Enable interrupts from this module */
    DMTimerIntEnable(SOC_DMTIMER_3_REGS, DMTIMER_INT_OVF_EN_FLAG);
    
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
	    /* Increment microsecond ticks */
	    clk.us_ticks++;
	    /* Increment intermediate counter */
	    clk.intermediate_us++;
	    /* If the intermediate couter hits 1000,
	     increment the millisecond ticks */
	    if (clk.intermediate_us == 1000) {
		clk.ms_ticks++;
		clk.intermediate_us = 0;
	    }
            clksrv_undelay(&clk);
            break;
        case CLKMSG_TIME:
            rply = clk.ms_ticks;
            rc = Reply(client, &rply, sizeof (rply));
            assertv(rc, rc == 0);
            break;
        case CLKMSG_DELAY:
            clksrv_delayuntil(&clk, client, clk.ms_ticks + msg.ticks);
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
    clk->us_ticks = 0;
    clk->ms_ticks = 0;
    clk->intermediate_us = 0;

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

    rc = clock_init();
    assertv(rc, rc == 0);

    rc = RegisterEvent(IRQ_CLOCK_TICK, &clksrv_notify_cb);
    assert(rc == 0);

    /* Start timer */
    DMTimerEnable(SOC_DMTIMER_3_REGS);

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
    DMTimerDisable(SOC_DMTIMER_3_REGS);
}

static int
clksrv_notify_cb(void *ptr, size_t n)
{
    assertv(ptr, ptr == NULL);
    assertv(n,   n   == 0);
    /* Clear the timer interrupt */
    DMTimerIntDisable(SOC_DMTIMER_3_REGS, DMTIMER_INT_OVF_EN_FLAG);
    DMTimerIntStatusClear(SOC_DMTIMER_3_REGS, DMTIMER_INT_OVF_IT_FLAG);
    DMTimerIntEnable(SOC_DMTIMER_3_REGS, DMTIMER_INT_OVF_EN_FLAG);
    return 0;
}

static void
clksrv_delayuntil(struct clksrv *clk, tid_t who, int when_ticks)
{
    if (when_ticks > clk->ms_ticks) {
        /* Add to priority queue */
        int rc;
        clk->tids[who & 0xff] = who;
        rc = pqueue_add(&clk->delays, who & 0xff, when_ticks);
        assertv(rc, rc == 0); /* we should always have enough space */
    } else {
        /* Reply immediately */
        int rc, rply;
        rply = when_ticks == clk->ms_ticks ? CLOCK_OK : CLOCK_DELAY_PAST;
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
        if (delay->key > clk->ms_ticks)
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
