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

#include "xarg.h"
#include "bwio.h"

void
u_init_main(void)
{
    tid_t ns_tid, clk_tid, serial_tid;
    struct serialcfg serialcfg;
    struct serialctx ctx;
    int rplylen;
    int ch;

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
    serial_tid = Create(2, &serialsrv_main);
    assertv(serial_tid, serial_tid >= 0);
    serialcfg.uart  = COM2;
    serialcfg.fifos = false;
    serialcfg.nocts = true;
    rplylen = Send(serial_tid, &serialcfg, sizeof (serialcfg), NULL, 0);
    assertv(rplylen, rplylen == 0);

    /* Read characters */
    serialctx_init(&ctx, COM2);
    while ((ch = Getc(&ctx)) != '\e') {
        bwputc(COM2, (char)ch);
    }

    Shutdown();
}

