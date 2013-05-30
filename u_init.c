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

static void u_test_main(void);

void
u_init_main(void)
{
    char  rply[128];
    tid_t ns_tid, child_tid;
    int   rc;

    /* Start the name server. It's important that startup proceeds so that
     * the TID of the name server can be known at compile time (NS_TID).
     * The priority strikes me as relatively unimportant - NS does some
     * work on startup and then probably never again. */
    ns_tid = Create(U_INIT_PRIORITY - 1, &ns_main);
    assert(ns_tid == NS_TID);

    /* Send/Receive/Reply test */
    bwputstr(COM2, "creating child\n");
    rc = Create(U_INIT_PRIORITY + 1, &u_test_main); /* lower priority */
    assert(rc >= 0);

    bwputstr(COM2, "WhoIs(child)\n");
    child_tid = WhoIs("child");
    assert(child_tid == rc);

    bwputstr(COM2, "sending\n");
    int rplylen = Send(child_tid, "The quick brown fox jumped over the lazy dog.", 46, rply, sizeof (rply));
    bwprintf(COM2, "got reply: %d: %s\n", rplylen, rply);
    bwputstr(COM2, "parent quitting\n");
}

static void
u_test_main(void)
{
    char  msg[128];
    tid_t sending_tid;
    int   rc, msglen;

    bwputstr(COM2, "RegisterAs(child)\n");
    rc = RegisterAs("child");
    assert(rc == 0);

    bwputstr(COM2, "receiving\n");
    msglen = Receive(&sending_tid, msg, sizeof (msg));
    bwprintf(COM2, "received from %d: %d: %s\n", sending_tid, msglen, msg);
    bwputstr(COM2, "replying\n");
    Reply(sending_tid, "Quick like a fox!", 18);
    bwputstr(COM2, "quitting\n");
}
