#include "config.h"
#include "u_tid.h"
#include "u_init.h"
#include "u_syscall.h"
#include "ns.h"

#include "xbool.h"
#include "xint.h"
#include "xassert.h"
#include "cpumode.h"

#include "xarg.h"
#include "bwio.h"

#include "u_rps_common.h"
#include "u_rpss.h"
#include "u_rpsc.h"

void
u_init_main(void)
{
    //char  rply[128];
    tid_t ns_tid, gs_tid, cli_tid1, cli_tid2, cli_tid3, cli_tid4, sd_tid;
    //int   rc;

    /* Start the name server. It's important that startup proceeds so that
     * the TID of the name server can be known at compile time (NS_TID).
     * The priority strikes me as relatively unimportant - NS does some
     * work on startup and then probably never again. */
    ns_tid = Create(U_INIT_PRIORITY - 1, &ns_main);
    assert(ns_tid == NS_TID);

    gs_tid = Create(U_INIT_PRIORITY + 1, &u_rpss_main);
    cli_tid1 = Create(U_INIT_PRIORITY + 1, &u_rpsc_main);
    cli_tid2 = Create(U_INIT_PRIORITY + 1, &u_rpsc_main);
    cli_tid3 = Create(U_INIT_PRIORITY + 1, &u_rpsc_main);
    cli_tid4 = Create(U_INIT_PRIORITY + 1, &u_rpsc_main);
    assert(gs_tid >= 0);
    assert(cli_tid1 >= 0);
    assert(cli_tid2 >= 0);
    assert(cli_tid3 >= 0);
    assert(cli_tid4 >= 0);

    sd_tid = Create(U_INIT_PRIORITY + 2, &Shutdown);
    assert(sd_tid >= 0);
}

