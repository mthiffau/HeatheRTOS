#include "u_tid.h"
#include "clock_srv.h"
#include "blnk_srv.h"

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "pqueue.h"

#include "xassert.h"
#include "u_syscall.h"
#include "ns.h"
#include "array_size.h"

#include "soc_AM335x.h"
#include "hw_types.h"
#include "gpio.h"

#include "float_test.h"

#include "xarg.h"
#include "bwio.h"

void 
fpu_test_main()
{
    tid_t mytid = MyTid();

    struct clkctx clk;
    clkctx_init(&clk);

    volatile float a, b;

    uint32_t time = Time(&clk);

    while(1) {

	if (time > 60000) {
	    bwprintf("TID: %d called shutdown\n\r", mytid);
	    Shutdown();
	}

	a = 0.5;
	b = 0.5;

	assert(a + b == 1.0);
	assert(a - b == 0.0);
	assert(a * b == 0.25);
	assert(a / b == 1.0);

	time = Time(&clk);
	Pass();
    }
}
