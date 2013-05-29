#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "kern.h"

int
main(void)
{
    return kern_main();
}
