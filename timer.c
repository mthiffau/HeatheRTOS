#include "xbool.h"
#include "xint.h"
#include "timer.h"

#include "xdef.h"
#include "static_assert.h"
#include "ts7200.h"

struct tmr40 {
    uint32_t low;
    uint32_t hien;
};

STATIC_ASSERT(t40_low_offs,  offsetof(struct tmr40, low)  == 0);
STATIC_ASSERT(t40_hien_offs, offsetof(struct tmr40, hien) == 4);

#define TMR40 ((volatile struct tmr40*)TIMER4_BASE)
#define TMR40_ENABLE (1 << 8)

struct tmr32 {
    uint32_t load;
    uint32_t val;
    uint32_t ctrl;
    uint32_t clr;
};

STATIC_ASSERT(t32_load_offs, offsetof(struct tmr32, load) == TIMER_LDR_OFFSET);
STATIC_ASSERT(t32_val_offs,  offsetof(struct tmr32, val)  == TIMER_VAL_OFFSET);
STATIC_ASSERT(t32_ctrl_offs, offsetof(struct tmr32, ctrl) == TIMER_CRTL_OFFSET);

#define TMR32 ((volatile struct tmr32*)TIMER3_BASE)

void tmr40_reset(void)
{
    TMR40->hien = 0;             /* disable and clear the timer */
    TMR40->hien = TMR40_ENABLE;  /* re-enable the timer */
}

uint32_t tmr40_get(void)
{
    return TMR40->low;
}

void tmr32_enable(bool enable)
{
    int ctrl = TMR32->ctrl;
    if (enable)
        ctrl |= TIMER_ENABLE_MASK;
    else
        ctrl &= ~TIMER_ENABLE_MASK;

    TMR32->ctrl = ctrl;
}

void tmr32_intr_clear(void)
{
    TMR32->clr = 0x1;
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
