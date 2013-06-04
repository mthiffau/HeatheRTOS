#include "test/test_ipc_perf.h"

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "kern.h"

#include "xassert.h"
#include "l1cache.h"
#include "timer.h"
#include "u_syscall.h"

#include "xarg.h"
#include "bwio.h"

#define NTRIALS 10000

static int  g_msglen; /* HACK FIXME? */
static void measure_send_recv_reply(int msglen, bool send_first);
static void ipc_perf_sender(void);
static void ipc_perf_receiver(void);

void
test_ipc_perf(void)
{
    static int bytes[] = { 4, 64 };
    int send_first, size;
    bwputstr(COM2, "test_ipc_perf...\n");
    for (send_first = 1; send_first >= 0; send_first--) {
        for (size = 0; size < 2; size++)
            measure_send_recv_reply(bytes[size], send_first);
    }
}

static void
measure_send_recv_reply(int msglen, bool send_first)
{
    struct kparam kp = {
        .init       = &ipc_perf_sender,
        .init_prio  = send_first ? 4 : 12,
        .irq_enable = false
    };
    g_msglen = msglen;

    tmr40_reset();
    kern_main(&kp);
    bwprintf(COM2, "  %d B\t%s\t%d us\n",
        msglen,
        send_first ? "send first" : "recv first",
        (tmr40_get() / 983) * 1000 / NTRIALS);
}

static void
ipc_perf_sender(void)
{
    char  msg[64], rply[64]; /* Don't care about the contents here */
    tid_t tid;
    int   i, msglen;

    tid = Create(8, &ipc_perf_receiver);
    assert(tid >= 0);

    msglen = g_msglen;
    for (i = 0; i < NTRIALS; i++)
        Send(tid, &msg, msglen, &rply, sizeof (rply));
}

static void
ipc_perf_receiver(void)
{
    char msg[64];
    int i, msglen;
    tid_t sender;
    for (i = 0; i < NTRIALS; i++) {
        msglen = Receive(&sender, &msg, sizeof (msg));
        Reply(sender, &msg, msglen);
    }
}
