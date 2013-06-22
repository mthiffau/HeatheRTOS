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
    struct serialctx tty;
    struct clkctx clock;
    int rplylen;
    int ch;

    char buf[512];
    int pos;

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
    serialcfg = (struct serialcfg) {
        .uart   = COM1,
        .fifos  = false,
        .nocts  = false,
        .baud   = 2400,
        .parity = false,
        .parity_even = false,
        .stop2  = true,
        .bits   = 8
    };
    rplylen = Send(serial_tid, &serialcfg, sizeof (serialcfg), NULL, 0);
    assertv(rplylen, rplylen == 0);

    /* Read characters */
    clkctx_init(&clock);
    serialctx_init(&tty, COM1);

    pos = 0;
    while ((ch = Getc(&tty)) != '\e') {
        buf[pos++] = ch;
        Putc(&tty, ch == '\r' ? '\n' : ch);
        if (ch == '-')
            Delay(&clock, 100);
        else if (ch == '\n' || ch == '\r') {
            int i;
            for (i = 0; i < pos - 1; i++) {
                Putc(&tty, buf[i]);
            }
            Putc(&tty, '\n');
            pos = 0;
        }
    }

    Shutdown();
}
