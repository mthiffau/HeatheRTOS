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
    struct clkctx clk;
    clkctx_init(&clk);

    volatile float a, b, c, d;

    uint32_t time = Time(&clk);

    while(1) {

	time = Time(&clk);
	if (time >= 300000)
	    Shutdown();

	c = (float)time;
	d = c / 1000.0 / 60.0;
	assert(d <= c);
	assert(d <= 5.0);

	a = 0.5;
	b = 0.5;

	assert(a + b == 1.0);
	assert(a - b == 0.0);
	assert(a * b == 0.25);
	assert(a / b == 1.0);

	assert((a * a) + (b * b) == a);
	assert((b * b) + (a * a) == b);

	a = 1.5;
	b = 0.5;

	assert(a > b);
	assert(b < a);
	assert(a / b == 3.0);
	assert(a * b == 0.75);

	assert(a / 0.0 == __builtin_inf());

	Pass();
    }
}
