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

static void test_clksrv_more_init(void);
static void client_main(void);

struct clientparam {
    int delayticks;
    int numdelays;
};

static char           clksrv_log_buf[512];
static struct testlog clksrv_log;
static const char    *clksrv_expected =
    "5 10 0\n5 10 1\n6 23 0\n5 10 2\n7 33 0\n5 10 3\n6 23 1\n5 10 4\n5 10 5\n7 33 1\n6 23 2\n5 10 6\n8 71 0\n5 10 7\n5 10 8\n6 23 3\n7 33 2\n5 10 9\n5 10 10\n6 23 4\n5 10 11\n5 10 12\n7 33 3\n6 23 5\n5 10 13\n8 71 1\n5 10 14\n5 10 15\n6 23 6\n7 33 4\n5 10 16\n5 10 17\n6 23 7\n5 10 18\n7 33 5\n5 10 19\n6 23 8\n8 71 2\n";

void
test_clksrv_more(void)
{
    struct kparam kp = {
        .init      = &test_clksrv_more_init,
        .init_prio = 8,
        .show_top  = false
    };
    bwputstr(COM2, "test_clksrv_more...");
    tlog_init(&clksrv_log, clksrv_log_buf, ARRAY_SIZE(clksrv_log_buf));
    kern_main(&kp);
    tlog_check(&clksrv_log, clksrv_expected);
    bwputstr(COM2, "ok\n");
}

static void
test_clksrv_more_init(void)
{
    static const int client_priority[4] = { 3, 4, 5, 6 };
    static const struct clientparam client_params[4] = {
        { .delayticks = 10, .numdelays = 20 },
        { .delayticks = 23, .numdelays =  9 },
        { .delayticks = 33, .numdelays =  6 },
        { .delayticks = 71, .numdelays =  3 }
    };

    tid_t ns_tid, clk_tid, client_tid;
    int i;

    /* Start the name server. It's important that startup proceeds so that
     * the TID of the name server can be known at compile time (NS_TID).
     * The priority strikes me as relatively unimportant - NS does some
     * work on startup and then probably never again. */
    ns_tid = Create(7, &ns_main);
    assertv(ns_tid, ns_tid == NS_TID);

    /* Start clock server */
    clk_tid = Create(7, &clksrv_main);
    assertv(clk_tid, clk_tid >= 0);

    /* Start test clients */
    for (i = 0; i < 4; i++) {
        client_tid = Create(client_priority[i], &client_main);
        assertv(client_tid, client_tid >= 0);
    }

    /* Four start-up requests */
    for (i = 0; i < 4; i++) {
        int rc;
        tid_t client;
        rc = Receive(&client, NULL, 0);
        assertv(rc, rc == 0);
        rc = Reply(client, &client_params[i], sizeof (client_params[i]));
        assert(rc == 0);
    }

    /* Four done requests */
    for (i = 0; i < 4; i++) {
        int rc;
        char msg;
        tid_t client;
        rc = Receive(&client, &msg, 1);
        assertv(rc, rc == 1);
        rc = Reply(client, &client_params[i], sizeof (client_params[i]));
        assert(rc == 0);
    }

    Shutdown();
}

static void
client_main(void)
{
    tid_t self, parent;
    int i, rplylen;
    struct clientparam param;
    struct clkctx clkctx;

    self   = MyTid();
    parent = MyParentTid();

    rplylen = Send(parent, NULL, 0, &param, sizeof (param));
    assertv(rplylen, rplylen == sizeof (param));

    clkctx_init(&clkctx); /* Finds clock server using WhoIs */
    for (i = 0; i < param.numdelays; i++) {
        int rc;
        rc = Delay(&clkctx, param.delayticks);
        assertv(rc, rc == 0);
        tlog_printf(&clksrv_log, "%d %d %d\n", self, param.delayticks, i);
    }

    /* Signal done */
    rplylen = Send(parent, &i, 1, &param, sizeof (param));
    assertv(rplylen, rplylen == sizeof (param));
}
