#include "xbool.h"
#include "xint.h"
#include "intr.h"

#include "xdef.h"
#include "static_assert.h"
#include "bithack.h"

#include "xarg.h"
#include "bwio.h"

#include "interrupt.h"

void
intr_enable(int intr, bool enable)
{
    if(enable) {
	IntSystemEnable(intr);
    } else {
	IntSystemDisable(intr);
    }
}

void
intr_config(int intr, unsigned int prio, bool fiq)
{
    IntPrioritySet(intr, 
		   prio, 
		   fiq ? AINTC_HOSTINT_ROUTE_FIQ : AINTC_HOSTINT_ROUTE_IRQ);
}

int
intr_cur()
{
    return IntActiveIrqNumGet();
}

void
intr_reset()
{
    IntMasterIRQDisable();
    IntMasterIRQEnable();
    IntAINTCInit();
    IntProtectionEnable();
}
