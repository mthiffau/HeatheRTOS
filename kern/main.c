#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "event.h"
#include "kern.h"

#include "l1cache.h"
#include "timer.h"

int
main(void)
{
    l1cache_enable();
    tmr40_reset();
    return kern_main(&def_kparam);
}
