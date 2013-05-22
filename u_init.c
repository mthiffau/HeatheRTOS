#include "u_tid.h"
#include "u_init.h"
#include "u_syscall.h"

#include "xint.h"
#include "cpumode.h"

#include "xarg.h"
#include "bwio.h"

static void spawn_test(int priority);
static void u_test_main(void);

void
u_init_main(void)
{
    /* Spawn two tasks at lower priority than init. */
    spawn_test(U_INIT_PRIORITY + 1);
    spawn_test(U_INIT_PRIORITY + 1);

    /* Spawn two tasks at higher priority than init. */
    spawn_test(U_INIT_PRIORITY - 1);
    spawn_test(U_INIT_PRIORITY - 1);

    /* Print and exit */
    bwputstr(COM2, "First: exiting\n");
}

static void
spawn_test(int priority)
{
    tid_t tid = Create(priority, &u_test_main);
    if (tid >= 0) {
        bwprintf(COM2, "Created: %d\n", tid);
        return;
    }

    switch (tid) {
    case -1:
        bwputstr(COM2, "error: Create: bad priority\n");
        return;
    case -2:
        bwputstr(COM2, "error: Create: no more task descriptors\n");
        return;
    default:
        bwputstr(COM2, "error: Create: unknown error\n");
        return;
    }
}

static void
u_test_main(void)
{
    tid_t self, parent;
    self   = MyTid();
    parent = MyParentTid();
    bwprintf(COM2, "MyTid:%d MyParentTid:%d\n", self, parent);
    Pass();
    bwprintf(COM2, "MyTid:%d MyParentTid:%d\n", self, parent);
}
