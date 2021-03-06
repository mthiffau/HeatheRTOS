#undef NOASSERT

#include "test/test_nsblk.h"

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
#include "ns.h"

#include "xarg.h"
#include "bwio.h"
#include "test/log.h"

#define NSBLK_LOGSIZE 128

static char           nsblk_log_buf[NSBLK_LOGSIZE];
static struct testlog nsblk_log;
static void nsblk_log_call(char call, int x, const char *name);
static void nsblk_log_call_r(char call, int x, const char *name, int r);

/* Log messages:
 * E         - init exits
 * Wx:name   - task x before WhoIs(name)
 * Wx:name>y - task x WhoIs(name) returned y
 * Rx:name   - task x before RegisterAs(name)
 * Rx:name>y - task x RegisterAs(name) returned y
 * S         - shutdown
 */
static const char *nsblk_log_expected[] = {
    /* message sequence for high-priority name server */
    "E;"
    "W3:spam;"
    "W4:spam;"
    "R5:eggs;"
    "R5:eggs>0;"
    "R6:spam;"
    "W4:spam>6;"
    "W3:spam>6;"
    "R6:spam>0;"
    "W7:eggs;"
    "W8:spam;"
    "W7:eggs>5;"
    "W8:spam>6;",
    /* message sequence for low-priority name server */
    "E;"
    "W3:spam;"
    "W4:spam;"
    "R5:eggs;"
    "R6:spam;"
    "W7:eggs;"
    "W8:spam;"
    "R5:eggs>0;"
    "R6:spam>0;"
    "W4:spam>6;"
    "W3:spam>6;"
    "W7:eggs>5;"
    "W8:spam>6;"
};

static void nsblk_init_main(void);
static void nsblk_spawn(int priority, void (*)(void), int expected_tid);

static void nsblk_who(const char *name, int id);
static void nsblk_reg(const char *name, int id);

static void nsblk_spam_who(int id) { nsblk_who("spam", id); }
static void nsblk_spam_reg(int id) { nsblk_reg("spam", id); }
static void nsblk_eggs_who(int id) { nsblk_who("eggs", id); }
static void nsblk_eggs_reg(int id) { nsblk_reg("eggs", id); }

static void nsblk_spam_who3(void) { nsblk_spam_who(3); }
static void nsblk_spam_who4(void) { nsblk_spam_who(4); }
static void nsblk_eggs_reg5(void) { nsblk_eggs_reg(5); }
static void nsblk_spam_reg6(void) { nsblk_spam_reg(6); }
static void nsblk_eggs_who7(void) { nsblk_eggs_who(7); }
static void nsblk_spam_who8(void) { nsblk_spam_who(8); }

static bool nsblk_low_prio; /* HACK FIXME? */
static void test_nsblk(bool low_prio);

void
test_nsblk_all(void)
{
    test_nsblk(false);
    test_nsblk(true);
}

static void
test_nsblk(bool low_prio)
{
    static struct kparam kp = {
        .init      = &nsblk_init_main,
        .init_prio = 0,
        .show_top  = false
    };

    bwprintf(COM2, "test_nsblk%s...", low_prio ? "_low_prio" : "");
    tlog_init(&nsblk_log, nsblk_log_buf, NSBLK_LOGSIZE);
    nsblk_low_prio = low_prio;
    kern_main(&kp);
    tlog_check(&nsblk_log, nsblk_log_expected[low_prio ? 1 : 0]);
    bwputstr(COM2, "ok\n");
}

static void
nsblk_init_main(void)
{
    nsblk_spawn(nsblk_low_prio ? 8 : 1, &ns_main, NS_TID);
    nsblk_spawn(2, &nsblk_spam_who3, 3);
    nsblk_spawn(2, &nsblk_spam_who4, 4);
    nsblk_spawn(3, &nsblk_eggs_reg5, 5);
    nsblk_spawn(4, &nsblk_spam_reg6, 6);
    nsblk_spawn(5, &nsblk_eggs_who7, 7);
    nsblk_spawn(5, &nsblk_spam_who8, 8);
    tlog_putstr(&nsblk_log, "E;");
}

static void
nsblk_spawn(int priority, void (*entry)(void), tid_t expected_tid)
{
    tid_t tid = Create(priority, entry);
    assert(tid == expected_tid);
}

static void
nsblk_who(const char *name, int id)
{
    tid_t srv_tid;
    nsblk_log_call('W', id, name);
    srv_tid = WhoIs(name);
    nsblk_log_call_r('W', id, name, srv_tid);
}

static void
nsblk_reg(const char *name, int id)
{
    int rc;
    nsblk_log_call('R', id, name);
    rc = RegisterAs(name);
    assert(rc == 0);
    nsblk_log_call_r('R', id, name, rc);
}

static void nsblk_log_call(char call, int x, const char *name)
{
    tlog_printf(&nsblk_log, "%c%d:%s;", call, x, name);
}

static void nsblk_log_call_r(char call, int x, const char *name, int r)
{
    tlog_printf(&nsblk_log, "%c%d:%s>%d;", call, x, name, r);
}
