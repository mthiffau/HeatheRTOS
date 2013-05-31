#include "xbool.h"
#include "xint.h"
#include "xassert.h"
#include "xarg.h"
#include "bwio.h"

#include "l1cache.h"
#include "timer.h"

#include "test/test_xmemcpy.h"
#include "test/test_ipc.h"
#include "test/test_nsblk.h"
#include "test/test_ipc_perf.h"

int
main(void)
{
    bwsetfifo(COM2, OFF);
    l1cache_enable();

    test_xmemcpy_all();
    test_ipc_all();
    test_nsblk_all();
    test_ipc_perf();

    return 0;
}

