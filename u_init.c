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

void
u_init_main(void)
{
    tid_t ns_tid, clk_tid;

    /* Start the name server. It's important that startup proceeds so that
     * the TID of the name server can be known at compile time (NS_TID).
     * The priority strikes me as relatively unimportant - NS does some
     * work on startup and then probably never again. */
    ns_tid = Create(U_INIT_PRIORITY - 1, &ns_main);
    assertv(ns_tid, ns_tid == NS_TID);

    /* Start clock server */
    clk_tid = Create(U_INIT_PRIORITY, &clksrv_main);
    assertv(clk_tid, clk_tid >= 0);

    int oldtime = -1;
    struct clkctx clkctx;
    clkctx_init(&clkctx);
    for (;;) {
        int rc, time = Time(&clkctx);
        if (time != oldtime) {
            oldtime = time;
            bwprintf(COM2, "new time %d\n", time);
            if (time >= 10)
                Shutdown();
        } else {
            bwputc(COM2, '.');
        }
        rc = Delay(&clkctx, 1);
        assertv(rc, rc == 0);
    }
}
