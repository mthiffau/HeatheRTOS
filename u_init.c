#include "u_tid.h"
#include "u_init.h"
#include "u_syscall.h"

#include "xint.h"
#include "cpumode.h"

#include "xarg.h"
#include "bwio.h"

static void u_test_main(void);

void
u_init_main(void)
{
    char rply[128];
    tid_t child = Create(U_INIT_PRIORITY - 1, &u_test_main);
    bwputstr(COM2, "sending\n");
    int rplylen = Send(child, "The quick brown fox jumped over the lazy dog.", 46, rply, sizeof (rply));
    bwprintf(COM2, "got reply: %d: %s\n", rplylen, rply);
    bwputstr(COM2, "parent quitting\n");
}

static void
u_test_main(void)
{
    char  msg[128];
    tid_t sending_tid;
    int   msglen;

    bwputstr(COM2, "receiving\n");
    msglen = Receive(&sending_tid, msg, sizeof (msg));
    bwprintf(COM2, "received from %d: %d: %s\n", sending_tid, msglen, msg);
    bwputstr(COM2, "replying\n");
    Reply(sending_tid, "Quick like a fox!", 18);
    bwputstr(COM2, "quitting\n");
}
