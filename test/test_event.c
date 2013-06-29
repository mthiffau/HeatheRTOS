#undef NOASSERT

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "event.h"
#include "kern.h"

#include "u_syscall.h"
#include "xassert.h"
#include "cpumode.h"

#include "xarg.h"
#include "bwio.h"
#include "test/log.h"

#include "clock_srv.h"
#include "timer.h"
#include "intr.h"

#define EVLOG_BUFSIZE   128

static void test_event(void);
static void test_event_main(void);
static void u_clock(void);
static int  u_clock_cb(void*, size_t);
static void foo(void);
static int  foo_cb(void*, size_t);
static void bar(void);
static int  bar_cb(void*, size_t);

static char evlog_buf[EVLOG_BUFSIZE];
static struct testlog evlog;

static const char *expected =
    "tick 1\n"
    "tick 2\n"
    "tick 3\n"
    "tick 4\n"
    "foo 1\n"
    "bar 1\n"
    "tick 5\n"
    "tick 6\n"
    "tick 7\n"
    "tick 8\n"
    "foo 2\n"
    "bar 2\n"
    "tick 9\n";

void
test_event_all(void)
{
    test_event();
}

static void
test_event(void)
{
    struct kparam kp = {
        .init      = &test_event_main,
        .init_prio = 8,
        .show_top  = false
    };
    bwputstr(COM2, "test_event...");
    tlog_init(&evlog, evlog_buf, EVLOG_BUFSIZE);
    kern_main(&kp);
    tlog_check(&evlog, expected);
    bwputstr(COM2, "ok\n");
}

static void
test_event_main(void)
{
    Create(0, &u_clock);
    Create(0, &foo);
    Create(0, &bar);
}

static void
u_clock(void)
{
    int ticks, rc;

    clock_init(100);
    rc = RegisterEvent(51, &u_clock_cb);
    assert(rc == 0);

    ticks = 0;
    while (ticks < 9) {
        rc = AwaitEvent((void*)0xdeadbeef, 10);
        assert(rc == 42);
        tlog_printf(&evlog, "tick %d\n", ++ticks);
        if (ticks % 4 == 0)
            intr_assert_mask(VIC1, (1 << 21) | (1 << 22), true);
    }

    Shutdown(); /* Event-blocked foo, bar will keep kernel running */
}

static int
u_clock_cb(void *p, size_t n)
{
    assert(p == (void*)0xdeadbeef);
    assert(n == 10);
    tmr32_intr_clear();
    return 42;
}

static void
foo(void)
{
    int ticks, rc;

    rc = RegisterEvent(21, &foo_cb);
    assert(rc == 0);

    ticks = 0;
    for (;;) {
        rc = AwaitEvent((void*)0xdeadbee5, 129);
        assert(rc == 7);
        tlog_printf(&evlog, "foo %d\n", ++ticks);
    }
}

static int
foo_cb(void *p, size_t n)
{
    assert(p == (void*)0xdeadbee5);
    assert(n == 129);
    intr_assert(VIC1, 21, false);
    return 7;
}

static void
bar(void)
{
    int ticks, rc;

    rc = RegisterEvent(22, &bar_cb);
    assert(rc == 0);

    ticks = 0;
    for (;;) {
        rc = AwaitEvent((void*)0xf00, 84);
        assert(rc == 11);
        tlog_printf(&evlog, "bar %d\n", ++ticks);
    }
}

static int
bar_cb(void *p, size_t n)
{
    assert(p == (void*)0xf00);
    assert(n == 84);
    intr_assert(VIC1, 22, false);
    return 11;
}
