#include "xbool.h"
#include "xassert.h"
#include "xarg.h"
#include "bwio.h"

#include "test/test_xmemcpy.h"
#include "test/test_ipc.h"
#include "test/test_nsblk.h"

int
main(void)
{
    bwsetfifo(COM2, OFF);
    test_xmemcpy_all();
    test_ipc_all();
    test_nsblk_all();
    return 0;
}

