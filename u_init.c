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
#include "serial_srv.h"
#include "ui_srv.h"

#include "xarg.h"
#include "bwio.h"

void
u_init_main(void)
{
    tid_t ns_tid, clk_tid, tty_tid, ui_tid;
    struct serialcfg ttycfg;
    int rplylen;

    /* Start the name server. It's important that startup proceeds so that
     * the TID of the name server can be known at compile time (NS_TID).
     * The priority strikes me as relatively unimportant - NS does some
     * work on startup and then probably never again. */
    ns_tid = Create(U_INIT_PRIORITY - 1, &ns_main);
    assertv(ns_tid, ns_tid == NS_TID);

    /* Start clock server */
    clk_tid = Create(2, &clksrv_main);
    assertv(clk_tid, clk_tid >= 0);

    /* Start serial server for TTY */
    tty_tid = Create(2, &serialsrv_main);
    assertv(tty_tid, tty_tid >= 0);
    ttycfg = (struct serialcfg) {
        .uart   = COM2,
        .fifos  = false,
        .nocts  = true,
        .baud   = 115200,
        .parity = false,
        .parity_even = false,
        .stop2  = false,
        .bits   = 8
    };
    rplylen = Send(tty_tid, &ttycfg, sizeof (ttycfg), NULL, 0);
    assertv(rplylen, rplylen == 0);

    /* Start UI server */
    ui_tid = Create(2, &uisrv_main);
    assertv(ui_tid, ui_tid >= 0);
}
