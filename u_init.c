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
    char  msg[128];
    tid_t sending_tid;
    int   msglen;

    Create(U_INIT_PRIORITY - 1, &u_test_main);
    bwputstr(COM2, "receiving\n");
    msglen = Receive(&sending_tid, msg, sizeof (msg));
    bwprintf(COM2, "received: %d: %s\n", msglen, msg);
}

static void
u_test_main(void)
{
    tid_t parent = MyParentTid();
    bwputstr(COM2, "sending\n");
    Send(parent, "The quick brown fox jumped over the lazy dog.", 46, (void*)0, 0);
    bwputstr(COM2, "quitting\n");
}
