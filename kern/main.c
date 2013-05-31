#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "kern.h"

#include "l1cache.h"

int
main(void)
{
    l1cache_enable();
    return kern_main(&def_kparam);
}
