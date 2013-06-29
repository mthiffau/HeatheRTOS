#include "config.h"
#include "xdef.h"
#include "u_tid.h"
#include "u_init.h"
#include "u_syscall.h"
#include "ns.h"

#include "xbool.h"
#include "xint.h"
#include "static_assert.h"
#include "sensor.h"
#include "xassert.h"
#include "cpumode.h"
#include "clock_srv.h"
#include "serial_srv.h"
#include "ui_srv.h"
#include "tcmux_srv.h"
#include "sensor_srv.h"

#include "xarg.h"
#include "bwio.h"

void
u_init_main(void)
{
    tid_t ns_tid, clk_tid, tty_tid, train_tid, tcmux_tid, sensor_tid, ui_tid;
    struct serialcfg ttycfg, traincfg;
    int rplylen;

    /* Start the name server. It's important that startup proceeds so that
     * the TID of the name server can be known at compile time (NS_TID).
     * The priority strikes me as relatively unimportant - NS does some
     * work on startup and then probably never again. */
    ns_tid = Create(PRIORITY_NS, &ns_main);
    assertv(ns_tid, ns_tid == NS_TID);

    /* Start clock server */
    clk_tid = Create(PRIORITY_CLOCK, &clksrv_main);
    assertv(clk_tid, clk_tid >= 0);

    /* Start serial server for TTY */
    tty_tid = Create(PRIORITY_SERIAL2, &serialsrv_main);
    assertv(tty_tid, tty_tid >= 0);
    ttycfg = (struct serialcfg) {
        .uart   = COM2,
        .fifos  = true,
        .nocts  = true,
        .baud   = 115200,
        .parity = false,
        .parity_even = false,
        .stop2  = false,
        .bits   = 8
    };
    rplylen = Send(tty_tid, &ttycfg, sizeof (ttycfg), NULL, 0);
    assertv(rplylen, rplylen == 0);

    /* Start serial server for the train controller. */
    train_tid = Create(PRIORITY_SERIAL1, &serialsrv_main);
    assertv(train_tid, train_tid >= 0);
    traincfg = (struct serialcfg) {
        .uart   = COM1,
        .fifos  = false,
        .nocts  = false,
        .baud   = 2400,
        .parity = false,
        .parity_even = false,
        .stop2  = true,
        .bits   = 8
    };
    rplylen = Send(train_tid, &traincfg, sizeof (traincfg), NULL, 0);
    assertv(rplylen, rplylen == 0);

    /* Start train control multiplexer */
    tcmux_tid = Create(PRIORITY_TCMUX, &tcmuxsrv_main);
    assertv(tcmux_tid, tcmux_tid >= 0);

    /* Start sensor server */
    sensor_tid = Create(PRIORITY_SENSOR, &sensrv_main);
    assertv(sensor_tid, sensor_tid >= 0);

    /* Start UI server */
    ui_tid = Create(PRIORITY_UI, &uisrv_main);
    assertv(ui_tid, ui_tid >= 0);
}
