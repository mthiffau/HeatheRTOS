#include "xbool.h"
#include "xint.h"
#include "intr.h"

#include "xdef.h"
#include "static_assert.h"

#include "xarg.h"
#include "bwio.h"

struct vic {
    uint32_t irqstat;       /* VICxIRQStatus    ro */
    uint32_t fiqstat;       /* VICxFIQStatus    ro */
    uint32_t rawstat;       /* VICxRawIntr      ro */
    uint32_t select;        /* VICxIntSelect    rw */
    uint32_t enable;        /* VICxIntEnable    rw */
    uint32_t enclear;       /* VICxIntEnClear   rw */
    uint32_t softint;       /* VICxSoftInt      rw */
    uint32_t softclear;     /* VICxSoftIntClear rw */
    uint32_t _unused0[0x4];
    uint32_t vectaddr;      /* VICxVectAddr     rw */
    uint32_t defvectaddr;   /* VICxDefVectAddr  rw */
    uint32_t _unused1[0x32];
    uint32_t vectaddrs[16]; /* VICxVectAddry    rw */
    uint32_t _unused3[0x30];
    uint32_t vectcntls[16]; /* VICxVectCntly    rw */
};

STATIC_ASSERT(vic_irqstat_offs,     offsetof(struct vic, irqstat    ) == 0x00);
STATIC_ASSERT(vic_fiqstat_offs,     offsetof(struct vic, fiqstat    ) == 0x04);
STATIC_ASSERT(vic_rawstat_offs,     offsetof(struct vic, rawstat    ) == 0x08);
STATIC_ASSERT(vic_select_offs,      offsetof(struct vic, select     ) == 0x0c);
STATIC_ASSERT(vic_enable_offs,      offsetof(struct vic, enable     ) == 0x10);
STATIC_ASSERT(vic_enclear_offs,     offsetof(struct vic, enclear    ) == 0x14);
STATIC_ASSERT(vic_softint_offs,     offsetof(struct vic, softint    ) == 0x18);
STATIC_ASSERT(vic_softclear_offs,   offsetof(struct vic, softclear  ) == 0x1c);
STATIC_ASSERT(vic_vectaddr_offs,    offsetof(struct vic, vectaddr   ) == 0x30);
STATIC_ASSERT(vic_defvectaddr_offs, offsetof(struct vic, defvectaddr) == 0x34);
STATIC_ASSERT(vic_vectaddrs_offs,   offsetof(struct vic, vectaddrs  ) == 0x100);
STATIC_ASSERT(vic_vectcntls_offs,   offsetof(struct vic, vectcntls  ) == 0x200);

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
vintr_set(volatile struct vic *vic, int vintr, int intr)
{
    volatile uint32_t *cntl = &vic->vectcntls[vintr];
    *cntl = (*cntl & ~0x1f) | (intr & 0x1f);
}

void
vintr_enable(volatile struct vic *vic, int vintr, bool enable)
{
    volatile uint32_t *cntl = &vic->vectcntls[vintr];
    if (enable)
        *cntl |= 1 << 5;
    else
        *cntl &= ~(1 << 5);
}

void
vintr_setdefisr(volatile struct vic *vic, uint32_t isr)
{
    vic->defvectaddr = isr;
}

void
vintr_setisr(volatile struct vic *vic, int vintr, uint32_t isr)
{
    vic->vectaddrs[vintr] = isr;
}

uint32_t
vintr_cur(volatile struct vic *vic)
{
    return vic->vectaddr;
}

void
vintr_clear(volatile struct vic *vic)
{
    vic->vectaddr = 0;
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
    int i;
    vic->select      = 0;          /* All interrupts act as IRQ   */
    vic->enable      = 0;          /* All interrupts disabled     */
    vic->softclear   = 0xffffffff; /* No soft interrupts asserted */
    vic->defvectaddr = 0;
    for (i = 0; i < 16; i++) {     /* No vectored interrupts      */
        vic->vectaddrs[i] = 0;
        vic->vectcntls[i] = 0;
    }
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
