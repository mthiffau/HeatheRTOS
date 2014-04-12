#include "xbool.h"
#include "xint.h"
#include "timer.h"

#include "xdef.h"
#include "static_assert.h"

#include "soc_AM335x.h"
#include "hw_cm_per.h"
#include "hw_cm_dpll.h"
#include "hw_types.h"

#include "dmtimer.h"

/* Fwd Decl. DMTimer2 Clock src enable/select */
void DMTimer2ModuleClkConfig(void);

/* Configure a timer to use for debug purposes */
void dbg_tmr_setup(void)
{
    /* Call the reset code */
    dbg_tmr_reset();
}

/* Reset the debug timer */
void dbg_tmr_reset(void)
{
    /* Disable timer */
    DMTimerDisable(SOC_DMTIMER_2_REGS);

    /* Load the counter with 0 */
    DMTimerCounterSet(SOC_DMTIMER_2_REGS, 0x0);

    /* Load the load register with the reload count value of 0 */
    DMTimerReloadSet(SOC_DMTIMER_2_REGS, 0x0);

    /* Make timer auto-reload, no compare */
    DMTimerModeConfigure(SOC_DMTIMER_2_REGS, DMTIMER_AUTORLD_NOCMP_ENABLE);

    /* Make sure pre-scaler clock is disabled */
    DMTimerPreScalerClkDisable(SOC_DMTIMER_2_REGS);

    /* Start the timer */
    DMTimerEnable(SOC_DMTIMER_2_REGS);
}

/* Get the value of the debug timer */
uint32_t dbg_tmr_get(void)
{
    return DMTimerCounterGet(SOC_DMTIMER_2_REGS);
}

