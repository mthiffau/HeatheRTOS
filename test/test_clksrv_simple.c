#undef NOASSERT

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "event.h"
#include "kern.h"

#include "xassert.h"
#include "cpumode.h"
#include "u_syscall.h"
#include "ns.h"
#include "clock_srv.h"

#include "array_size.h"
#include "xarg.h"
#include "bwio.h"
#include "test/log.h"

static void test_clksrv_simple_init(void);

static char           clksrv_log_buf[128];
static struct testlog clksrv_log;
static const char    *clksrv_expected =
    "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n";

void
test_clksrv_simple(void)
{
    struct kparam kp = {
        .init      = &test_clksrv_simple_init,
        .init_prio = 8
    };
    bwputstr(COM2, "test_clksrv_simple...");
    tlog_init(&clksrv_log, clksrv_log_buf, ARRAY_SIZE(clksrv_log_buf));
    kern_main(&kp);
    tlog_check(&clksrv_log, clksrv_expected);
    bwputstr(COM2, "ok\n");
}

static void
test_clksrv_simple_init(void)
{
    tid_t ns_tid, clk_tid;

    /* Start the name server. It's important that startup proceeds so that
     * the TID of the name server can be known at compile time (NS_TID).
     * The priority strikes me as relatively unimportant - NS does some
     * work on startup and then probably never again. */
    ns_tid = Create(7, &ns_main);
    assertv(ns_tid, ns_tid == NS_TID);

    /* Start clock server */
    clk_tid = Create(7, &clksrv_main);
    assertv(clk_tid, clk_tid >= 0);

    int oldtime = -1;
    struct clkctx clkctx;
    clkctx_init(&clkctx);
    for (;;) {
        int rc, time = Time(&clkctx);
        if (time != oldtime) {
            oldtime = time;
            tlog_printf(&clksrv_log, "%d\n", time);
            if (time >= 10)
                Shutdown();
        } else {
            tlog_putc(&clksrv_log, '.');
        }
        rc = Delay(&clkctx, 1);
        assertv(rc, rc == 0);
    }
}
