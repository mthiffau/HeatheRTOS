#include "test/test_nsblk.h"

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "kern.h"

#include "xassert.h"
#include "xstring.h"
#include "u_syscall.h"
#include "ns.h"

#include "xarg.h"
#include "bwio.h"

#define NSBLK_LOGSIZE 128

static char nsblk_log_data[NSBLK_LOGSIZE];
static int  nsblk_log_pos;
static void nsblk_log_init(void) { nsblk_log_pos = 0; }
static void nsblk_log(const char *data, int size)
{
    assert(nsblk_log_pos + size <= NSBLK_LOGSIZE);
    while (size-- > 0)
        nsblk_log_data[nsblk_log_pos++] = *data++;
}

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
    "W2:spam;"
    "W3:spam;"
    "R4:eggs;"
    "R4:eggs>0;"
    "R5:spam;"
    "W3:spam>5;"
    "W2:spam>5;"
    "R5:spam>0;"
    "W6:eggs;"
    "W7:spam;"
    "W6:eggs>4;"
    "W7:spam>5;"
    "S;",
    /* message sequence for low-priority name server */
    "E;"
    "W2:spam;"
    "W3:spam;"
    "R4:eggs;"
    "R5:spam;"
    "W6:eggs;"
    "W7:spam;"
    "R4:eggs>0;"
    "R5:spam>0;"
    "W3:spam>5;"
    "W2:spam>5;"
    "W6:eggs>4;"
    "W7:spam>5;"
    "S;"
};

static void nsblk_log_char(char c) { nsblk_log(&c, 1); }
static void nsblk_log_int(int x) { assert(x >= 0 && x <= 9); nsblk_log_char('0' + x); }
static void nsblk_log_call_noterm(char call, int x, const char *name)
{
    nsblk_log_char(call);
    nsblk_log_int(x);
    nsblk_log_char(':');
    nsblk_log(name, strnlen(name, NS_NAME_MAXLEN));
}
static void nsblk_log_call(char call, int x, const char *name)
{
    nsblk_log_call_noterm(call, x, name);
    nsblk_log_char(';');
}
static void nsblk_log_call_r(char call, int x, const char *name, int r)
{
    nsblk_log_call_noterm(call, x, name);
    nsblk_log_char('>');
    nsblk_log_int(r);
    nsblk_log_char(';');
}

static void nsblk_init_main(void);
static void nsblk_spawn(int priority, void (*)(void), int expected_tid);
static void nsblk_shutdown(void);

static void nsblk_who(const char *name, int id);
static void nsblk_reg(const char *name, int id);

static void nsblk_spam_who(int id) { nsblk_who("spam", id); }
static void nsblk_spam_reg(int id) { nsblk_reg("spam", id); }
static void nsblk_eggs_who(int id) { nsblk_who("eggs", id); }
static void nsblk_eggs_reg(int id) { nsblk_reg("eggs", id); }

static void nsblk_spam_who2(void) { nsblk_spam_who(2); }
static void nsblk_spam_who3(void) { nsblk_spam_who(3); }
static void nsblk_eggs_reg4(void) { nsblk_eggs_reg(4); }
static void nsblk_spam_reg5(void) { nsblk_spam_reg(5); }
static void nsblk_eggs_who6(void) { nsblk_eggs_who(6); }
static void nsblk_spam_who7(void) { nsblk_spam_who(7); }

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
        .init       = &nsblk_init_main,
        .init_prio  = 0,
        .irq_enable = false
    };

    bwprintf(COM2, "test_nsblk%s...", low_prio ? "_low_prio" : "");
    nsblk_log_init();
    nsblk_low_prio = low_prio;
    kern_main(&kp);
    nsblk_log_char('\0');
    assert(strcmp(nsblk_log_data, nsblk_log_expected[low_prio ? 1 : 0]) == 0);
    bwputstr(COM2, "ok\n");
}

static void
nsblk_init_main(void)
{
    nsblk_spawn(nsblk_low_prio ? 8 : 1, &ns_main, 1);
    nsblk_spawn(2, &nsblk_spam_who2, 2);
    nsblk_spawn(2, &nsblk_spam_who3, 3);
    nsblk_spawn(3, &nsblk_eggs_reg4, 4);
    nsblk_spawn(4, &nsblk_spam_reg5, 5);
    nsblk_spawn(5, &nsblk_eggs_who6, 6);
    nsblk_spawn(5, &nsblk_spam_who7, 7);
    nsblk_spawn(N_PRIORITIES - 1, &nsblk_shutdown, 8);
    nsblk_log("E;", 2);
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

void
nsblk_shutdown(void)
{
    nsblk_log("S;", 2);
    Shutdown();
}