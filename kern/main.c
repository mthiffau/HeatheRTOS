#include "config.h"
#include "xbool.h"
#include "xarg.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "event.h"
#include "kern.h"

#include "cache.h"
#include "pll.h"
#include "timer.h"
#include "bwio.h"

int
main(void)
{
    cache_enable();
    pll_setup();
    dbg_tmr_setup();
    bwio_uart_setup();
    return kern_main(&def_kparam);
}
