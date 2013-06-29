#undef NOASSERT

#include "test/test_ipc.h"

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "event.h"
#include "kern.h"

#include "xassert.h"
#include "xstring.h"
#include "u_syscall.h"

#include "xarg.h"
#include "bwio.h"

#define TEST(init) test_ipc_kern(#init, &init)

static void test_ipc_kern(const char *name, void (*)(void));

static void test_send_tid_impossible(void);
static void test_send_tid_nosuchtask(void);
static void test_rply_tid_impossible(void);
static void test_rply_tid_nosuchtask(void);
static void test_rply_notrplyblk(void);
static void test_msglen_send2recv(void);
static void test_recv_overflow(void);
static void test_msglen_rply2send(void);
static void test_rply_overflow(void);

void
test_ipc_all(void)
{
    TEST(test_send_tid_impossible);
    TEST(test_send_tid_nosuchtask);
    TEST(test_rply_tid_impossible);
    TEST(test_rply_tid_nosuchtask);
    TEST(test_rply_notrplyblk);
    TEST(test_msglen_send2recv);
    TEST(test_recv_overflow);
    TEST(test_msglen_rply2send);
    TEST(test_rply_overflow);
}

static void test_ipc_kern(const char *name, void (*init)(void))
{
    struct kparam kp = { .init = init, .init_prio = 8, .show_top = false };
    bwprintf(COM2, "%s...", name);
    kern_main(&kp);
    bwputstr(COM2, "ok\n");
}

static void
test_send_tid_impossible(void)
{
    int rc;
    rc = Send(128, NULL, 0, NULL, 0);
    assert(rc == -1);
    rc = Send(255, NULL, 0, NULL, 0);
    assert(rc == -1);
    rc = Send(256 + 128, NULL, 0, NULL, 0);
    assert(rc == -1);
    rc = Send(256 + 255, NULL, 0, NULL, 0);
    assert(rc == -1);
    rc = Send(1 << 16, NULL, 0, NULL, 0);
    assert(rc == -1);
    rc = Send(-1, NULL, 0, NULL, 0);
    assert(rc == -1);
}

static void
test_send_tid_nosuchtask(void)
{
    int rc;
    rc = Send(2, NULL, 0, NULL, 0);
    assert(rc == -2);
    rc = Send(3, NULL, 0, NULL, 0);
    assert(rc == -2);
    rc = Send(59, NULL, 0, NULL, 0);
    assert(rc == -2);
    rc = Send(127, NULL, 0, NULL, 0);
    assert(rc == -2);
    rc = Send(256, NULL, 0, NULL, 0);
    assert(rc == -2);
    rc = Send(256 + 127, NULL, 0, NULL, 0);
    assert(rc == -2);
    rc = Send(512, NULL, 0, NULL, 0);
    assert(rc == -2);
}

static void
test_rply_tid_impossible(void)
{
    int rc;
    rc = Reply(128, NULL, 0);
    assert(rc == -1);
    rc = Reply(255, NULL, 0);
    assert(rc == -1);
    rc = Reply(256 + 128, NULL, 0);
    assert(rc == -1);
    rc = Reply(256 + 255, NULL, 0);
    assert(rc == -1);
    rc = Reply(1 << 16, NULL, 0);
    assert(rc == -1);
    rc = Reply(-1, NULL, 0);
    assert(rc == -1);
}

static void
test_rply_tid_nosuchtask(void)
{
    int rc;
    rc = Reply(2, NULL, 0);
    assert(rc == -2);
    rc = Reply(3, NULL, 0);
    assert(rc == -2);
    rc = Reply(59, NULL, 0);
    assert(rc == -2);
    rc = Reply(127, NULL, 0);
    assert(rc == -2);
    rc = Reply(256, NULL, 0);
    assert(rc == -2);
    rc = Reply(256 + 127, NULL, 0);
    assert(rc == -2);
    rc = Reply(512, NULL, 0);
    assert(rc == -2);
}

static void
test_rply_notrplyblk(void)
{
    int rc;
    rc = Reply(0, NULL, 0);
    assert(rc == -3);
}

static void
test_msglen_send2recv_sender(void)
{
    Send(MyParentTid(), "foobar baz", 11, NULL, 0);
}

static void
test_msglen_send2recv(void)
{
    char msg[37] = "0123456789abcdefghijklmnopqrstuvwxyz";
    int msglen, child_tid, sender_tid;
    child_tid = Create(0, test_msglen_send2recv_sender);
    sender_tid = -42;
    msglen = Receive(&sender_tid, msg, sizeof (msg));
    assert(sender_tid == child_tid);
    assert(msglen == 11);
    assert(strcmp(msg, "foobar baz") == 0);
    assert(strcmp(msg + 11, "bcdefghijklmnopqrstuvwxyz") == 0);
}

static void
test_recv_overflow_sender(void)
{
    Send(MyParentTid(), "foobar baz", 11, NULL, 0);
}

static void
test_recv_overflow(void)
{
    char msg[37] = "0123456789abcdefghijklmnopqrstuvwxyz";
    int msglen, child_tid, sender_tid;
    child_tid = Create(0, test_recv_overflow_sender);
    sender_tid = -42;
    msglen = Receive(&sender_tid, msg, 10);
    assert(sender_tid == child_tid);
    assert(msglen == 11);
    assert(strcmp(msg, "foobar bazabcdefghijklmnopqrstuvwxyz") == 0);
}

static bool rply2send_sender_done;
static void
test_msglen_rply2send_sender(void)
{
    char reply[37] = "ZYXWVUTSRQPONMLKJIHGFEDCBA9876543210";
    int  replylen;
    replylen = Send(MyParentTid(), "foobar baz", 11, reply, sizeof (reply));
    assert(replylen == 9);
    assert(strcmp(reply, "bazinga!") == 0);
    assert(strcmp(reply + 9, "QPONMLKJIHGFEDCBA9876543210") == 0);
    rply2send_sender_done = true;
}

static void
test_msglen_rply2send(void)
{
    char msg[37] = "0123456789abcdefghijklmnopqrstuvwxyz";
    int msglen, child_tid, sender_tid, rc;
    rply2send_sender_done = false;
    child_tid = Create(0, test_msglen_rply2send_sender);
    sender_tid = -42;
    msglen = Receive(&sender_tid, msg, sizeof (msg));
    assert(sender_tid == child_tid);
    assert(msglen == 11);
    assert(strcmp(msg, "foobar baz") == 0);
    assert(strcmp(msg + 11, "bcdefghijklmnopqrstuvwxyz") == 0);
    rc = Reply(sender_tid, "bazinga!", 9);
    assert(rc == 0);
    assert(rply2send_sender_done);
}

static bool rply_overflow_sender_done;
static void
test_rply_overflow_sender(void)
{
    char reply[37] = "ZYXWVUTSRQPONMLKJIHGFEDCBA9876543210";
    int  replylen;
    replylen = Send(MyParentTid(), "foobar baz", 11, reply, 8);
    assert(replylen == 9);
    assert(strcmp(reply, "bazinga!RQPONMLKJIHGFEDCBA9876543210") == 0);
    rply_overflow_sender_done = true;
}

static void
test_rply_overflow(void)
{
    char msg[37] = "0123456789abcdefghijklmnopqrstuvwxyz";
    int msglen, child_tid, sender_tid, rc;
    rply_overflow_sender_done = false;
    child_tid = Create(0, &test_rply_overflow_sender);
    sender_tid = -42;
    msglen = Receive(&sender_tid, msg, sizeof (msg));
    assert(sender_tid == child_tid);
    assert(msglen == 11);
    assert(strcmp(msg, "foobar baz") == 0);
    assert(strcmp(msg + 11, "bcdefghijklmnopqrstuvwxyz") == 0);
    rc = Reply(sender_tid, "bazinga!", 9);
    assert(rc == -4);
    assert(rply2send_sender_done);
}
