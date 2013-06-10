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

static void client_main(void);

struct clientparam {
    int delayticks;
    int numdelays;
};

void
u_init_main(void)
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
    ns_tid = Create(U_INIT_PRIORITY - 1, &ns_main);
    assertv(ns_tid, ns_tid == NS_TID);

    /* Start clock server */
    clk_tid = Create(2, &clksrv_main);
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
        bwprintf(COM2, "%d %d %d\n", self, param.delayticks, i);
    }

    /* Signal done */
    rplylen = Send(parent, &i, 1, &param, sizeof (param));
    assertv(rplylen, rplylen == sizeof (param));
}
