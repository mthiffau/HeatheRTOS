#include "util.h"
#include "timer.h"

#include "ts7200.h"

struct tmr32 {
    uint32_t load;
    uint32_t val;
    uint32_t ctrl;
};

STATIC_ASSERT(load_offs, offsetof(struct tmr32, load) == TIMER_LDR_OFFSET);
STATIC_ASSERT(val_offs,  offsetof(struct tmr32, val)  == TIMER_VAL_OFFSET);
STATIC_ASSERT(ctrl_offs, offsetof(struct tmr32, ctrl) == TIMER_CRTL_OFFSET);

#define TMR32 ((volatile struct tmr32*)TIMER3_BASE)

void tmr32_enable(bool enable)
{
    int ctrl = TMR32->ctrl;
    if (enable)
        ctrl |= TIMER_ENABLE_MASK;
    else
        ctrl &= ~TIMER_ENABLE_MASK;

    TMR32->ctrl = ctrl;
}

int tmr32_set_kHz(int kHz)
{
    switch (kHz) {
    case 508:
        TMR32->ctrl |= TIMER_CLKSEL_MASK;
        return 0;

    case 2:
        TMR32->ctrl &= ~TIMER_CLKSEL_MASK;
        return 0;
    }

    return -1;
}

void tmr32_set_periodic(bool periodic)
{
    if (periodic)
        TMR32->ctrl |= TIMER_MODE_MASK;
    else
        TMR32->ctrl &= ~TIMER_MODE_MASK;
}

void tmr32_load(uint32_t x)
{
    TMR32->load = x;
}

uint32_t tmr32_get(void)
{
    return TMR32->val;
}
