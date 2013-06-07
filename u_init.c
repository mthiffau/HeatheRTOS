#include "config.h"
#include "xdef.h"
#include "u_tid.h"
#include "u_init.h"
#include "u_syscall.h"
#include "ns.h"

#include "xbool.h"
#include "xint.h"
#include "xassert.h"
#include "cpumode.h"
#include "clock_srv.h"

#include "xarg.h"
#include "bwio.h"

static void u_idle(void);

void
u_init_main(void)
{
    tid_t ns_tid, idle_tid, clk_tid;

    /* Start the name server. It's important that startup proceeds so that
     * the TID of the name server can be known at compile time (NS_TID).
     * The priority strikes me as relatively unimportant - NS does some
     * work on startup and then probably never again. */
    ns_tid = Create(U_INIT_PRIORITY - 1, &ns_main);
    assert(ns_tid == NS_TID);

    /* FIXME this should be special in the kernel */
    idle_tid = Create(PRIORITY_IDLE, &u_idle);
    assert(idle_tid >= 0);

    /* Start clock server */
    clk_tid = Create(U_INIT_PRIORITY, &clksrv_main);
    assert(clk_tid >= 0);

    int oldtime = -1;
    struct clkctx clkctx;
    clkctx_init(&clkctx);
    for (;;) {
        int time = Time(&clkctx);
        if (time != oldtime) {
            oldtime = time;
            bwprintf(COM2, "new time %d\n", time);
        }
    }
}

static void u_idle(void)
{
    for (;;) { }
}
