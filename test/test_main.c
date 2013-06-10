#include "xbool.h"
#include "xint.h"
#include "xarg.h"
#include "bwio.h"

#include "l1cache.h"
#include "timer.h"

#include "test/test_xmemcpy.h"
#include "test/test_pqueue.h"
#include "test/test_ipc.h"
#include "test/test_nsblk.h"
#include "test/test_event.h"
#include "test/test_clksrv_simple.h"
#include "test/test_ipc_perf.h"

int
main(void)
{
    bwsetfifo(COM2, OFF);
    l1cache_enable();

    test_xmemcpy_all();
    test_pqueue_all();
    test_ipc_all();
    test_nsblk_all();
    test_event_all();
    test_clksrv_simple();
    test_ipc_perf();

    return 0;
}

