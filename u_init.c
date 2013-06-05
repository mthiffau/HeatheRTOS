#include "config.h"
#include "xdef.h"
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

#include "clock.h"
#include "timer.h"
#include "intr.h"

static void u_idle(void);
static void u_clock(void);
static int  u_clock_cb(void*, size_t);
static void foo(void);
static int  foo_cb(void*, size_t);
static void bar(void);
static int  bar_cb(void*, size_t);

void
u_init_main(void)
{
    tid_t ns_tid, clock_tid, idle_tid;

    /* Start the name server. It's important that startup proceeds so that
     * the TID of the name server can be known at compile time (NS_TID).
     * The priority strikes me as relatively unimportant - NS does some
     * work on startup and then probably never again. */
    ns_tid = Create(U_INIT_PRIORITY - 1, &ns_main);
    assert(ns_tid == NS_TID);

    /* Interrupt test */
    clock_tid = Create(0, &u_clock);
    assert(clock_tid >= 0);

    Create(0, &foo);
    Create(0, &bar);

    idle_tid = Create(N_PRIORITIES - 1, &u_idle);
    assert(idle_tid >= 0);
}

static void
u_idle(void)
{
    for (;;) { }
}

static void
u_clock(void)
{
    struct clock clock;
    int          ticks, rc;

    clock_init(&clock, 4);
    rc = RegisterEvent(2, 51, &u_clock_cb);
    assert(rc == 0);

    ticks = 0;
    for (;;) {
        rc = AwaitEvent((void*)0xdeadbeef, 10);
        assert(rc == 42);
        bwprintf(COM2, "tick %d\n", ++ticks);
        if (ticks % 4 == 0)
            intr_assert_mask(VIC1, (1 << 21) | (1 << 22), true);
    }
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

    rc = RegisterEvent(10, 21, &foo_cb);
    assert(rc == 0);

    ticks = 0;
    for (;;) {
        rc = AwaitEvent((void*)0xdeadbee5, 129);
        assert(rc == 7);
        bwprintf(COM2, "foo %d\n", ++ticks);
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

    rc = RegisterEvent(9, 22, &bar_cb);
    assert(rc == 0);

    ticks = 0;
    for (;;) {
        rc = AwaitEvent((void*)0xf00, 84);
        assert(rc == 11);
        bwprintf(COM2, "bar %d\n", ++ticks);
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
