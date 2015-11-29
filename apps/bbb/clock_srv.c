/*******************************************************************************
    Copyright 2014 Matthew Thiffault

    This file is part of HeatheRTOS.

    HeatheRTOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    HeatheRTOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HeatheRTOS.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************/
/*
* Some of this code is pulled from TI's BeagleBone Black StarterWare
* Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
*
*  Redistribution and use in source and binary forms, with or without 
*  modification, are permitted provided that the following conditions 
*  are met:
*
*    Redistributions of source code must retain the above copyright 
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the 
*    documentation and/or other materials provided with the   
*    distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include "u_tid.h"
#include "clock_srv.h"

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "pqueue.h"

#include "xassert.h"
#include "u_syscall.h"
#include "ns.h"
#include "array_size.h"

#include "soc_AM335x.h"
#include "hw_types.h"
#include "hw_cm_per.h"
#include "hw_cm_dpll.h"
#include "interrupt.h"
#include "dmtimer.h"

#define GPIO_INSTANCE_ADDRESS (SOC_GPIO_1_REGS)
#define GPIO_INSTANCE_PIN_NUMBER (23)

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
    /* Clock ticks in milliseconds */
    int ms_ticks;

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
    /* Set up timer to produce interrupts every millisecond */
    /* DMTimers2-7 on BBB run off 24Mhz clock (CLK_M_OSC) */

    /* Configure functional clock */
    DMTimer3ModuleClkConfig();

    /* Disable module */
    DMTimerDisable(SOC_DMTIMER_3_REGS);

    /* Set pre-scaler value */
    //DMTimerPreScalerClkEnable(SOC_DMTIMER_3_REGS, DMTIMER_PRESCALER_CLK_DIV_BY_8);
    DMTimerPreScalerClkDisable(SOC_DMTIMER_3_REGS);

    /* Re-load timer counter to 1 ms */
    unsigned int reloadValue = CLOCK_OVF - (1 * CLOCK_1ms);
    DMTimerCounterSet(SOC_DMTIMER_3_REGS, reloadValue);

    /* Set re-load value for timer */
    DMTimerReloadSet(SOC_DMTIMER_3_REGS, reloadValue);

    /* Make timer auto-reload, compare mode */
    DMTimerModeConfigure(SOC_DMTIMER_3_REGS, DMTIMER_AUTORLD_NOCMP_ENABLE);

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

            clk.ms_ticks++;

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
    clk->ms_ticks = 0;

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

    //clksrv_cleanup();
    RegisterCleanup(&clksrv_cleanup);

    rc = clock_init();
    assertv(rc, rc == 0);

    rc = RegisterEvent(SYS_INT_TINT3, &clksrv_notify_cb);
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
