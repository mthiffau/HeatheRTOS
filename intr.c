#include "xbool.h"
#include "intr.h"

#include "xint.h"
#include "xdef.h"
#include "static_assert.h"

#include "xarg.h"
#include "bwio.h"

struct vic {
    uint32_t irqstat;   /* VICxIRQStatus    ro */
    uint32_t fiqstat;   /* VICxFIQStatus    ro */
    uint32_t rawstat;   /* VICxRawIntr      ro */
    uint32_t select;    /* VICxIntSelect    rw */
    uint32_t enable;    /* VICxIntEnable    rw */
    uint32_t enclear;   /* VICxIntEnClear   rw */
    uint32_t softint;   /* VICxSoftInt      rw */
    uint32_t softclear; /* VICxSoftIntClear rw */
};

STATIC_ASSERT(vic_irqstat_offs,   offsetof(struct vic, irqstat  ) == 0x00);
STATIC_ASSERT(vic_fiqstat_offs,   offsetof(struct vic, fiqstat  ) == 0x04);
STATIC_ASSERT(vic_rawstat_offs,   offsetof(struct vic, rawstat  ) == 0x08);
STATIC_ASSERT(vic_select_offs,    offsetof(struct vic, select   ) == 0x0c);
STATIC_ASSERT(vic_enable_offs,    offsetof(struct vic, enable   ) == 0x10);
STATIC_ASSERT(vic_enclear_offs,   offsetof(struct vic, enclear  ) == 0x14);
STATIC_ASSERT(vic_softint_offs,   offsetof(struct vic, softint  ) == 0x18);
STATIC_ASSERT(vic_softclear_offs, offsetof(struct vic, softclear) == 0x1c);

void
intr_enable(volatile struct vic *vic, int intr, bool enable)
{
    if (enable)
        vic->enable |= 1 << intr;
    else
        vic->enclear = 1 << intr;
}

void
intr_setfiq(volatile struct vic *vic, int intr, bool fiq)
{
    if (fiq)
        vic->select |= 1 << intr;
    else
        vic->select &= ~(1 << intr);
}

void
intr_reset_all(void)
{
    intr_reset(VIC1);
    intr_reset(VIC2);
}

void
intr_reset(volatile struct vic *vic)
{
    vic->select    = 0;          /* All interrupts act as IRQ   */
    vic->enable    = 0;          /* All interrupts disabled     */
    vic->softclear = 0xffffffff; /* No soft interrupts asserted */
}

void
vic_dbgprint(volatile struct vic *vic)
{
    bwprintf(COM2,
        "\trawstat: 0x%x\n"
        "\tselect:  0x%x\n"
        "\tenable:  0x%x\n"
        "\tirqstat: 0x%x\n",
        vic->rawstat,
        vic->select,
        vic->enable,
        vic->irqstat);
}
