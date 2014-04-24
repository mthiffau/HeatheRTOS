#include "xbool.h"
#include "xint.h"
#include "intr.h"

#include "xdef.h"
#include "static_assert.h"
#include "bithack.h"

#include "xarg.h"
#include "bwio.h"

#include "soc_AM335x.h"
#include "hw_intc.h"
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
    //IntProtectionEnable();
}

void
intr_acknowledge(void)
{
    volatile unsigned int* 
	intc_control_reg = (unsigned int*)(SOC_AINTC_REGS + INTC_CONTROL);
    *intc_control_reg = INTC_CONTROL_NEWIRQAGR;

    __asm__ volatile ("dsb":::"memory");
}
