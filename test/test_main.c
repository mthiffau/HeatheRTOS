#include "xbool.h"
#include "xassert.h"
#include "xarg.h"
#include "bwio.h"

#include "test/test_xmemcpy.h"

int
main(void)
{
    bwsetfifo(COM2, OFF);
    test_xmemcpy_all();
    return 0;
}

