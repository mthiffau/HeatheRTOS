#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "u_tid.h"
#include "config.h"
#include "event.h"

#include "intr.h"
#include "array_size.h"
#include "xassert.h"

#define IRQ_INVALID (-1)

static void split_irq(volatile struct vic**, int*);

void
evt_init(struct eventab *tab)
{
    unsigned i;
    for (i = 0; i < ARRAY_SIZE(tab->events); i++)
        tab->events[i].irq = IRQ_INVALID;
    for (i = 0; i < ARRAY_SIZE(tab->irq_used); i++)
        tab->irq_used[i] = 0;
}

int evt_register(
    struct eventab *tab,
    tid_t tid,
    int event,
    int irq,
    int (*cb)(void*, size_t))
{
    int mask_i, mask_j;
    struct event *evt;
    volatile struct vic *vic;

    evt = &tab->events[event];
    if (event < 0 || event >= IRQ_PRIORITIES)
        return EVT_OOR;
    if (evt->irq != IRQ_INVALID)
        return EVT_IN_USE;
    if (irq < 0 || irq >= IRQ_COUNT)
        return IRQ_OOR;

    mask_i = irq / 32;
    mask_j = irq % 32;
    if (tab->irq_used[mask_i] & (1 << mask_j))
        return IRQ_IN_USE;

    /* Set up the event */
    tab->irq_used[mask_i] |= 1 << mask_j;
    evt->irq = irq;
    evt->tid = tid;
    evt->cb  = cb;

    /* Set up the vectored interrupt slot event to refer to irq,
     * set its ISR address, and enable it. */
    split_irq(&vic, &irq);
    vintr_set(   vic, event, irq);
    vintr_setisr(vic, event, event);
    vintr_enable(vic, event, true);
    intr_setfiq(vic, irq, false);
    return 0;
}

int
evt_unregister(struct eventab *tab, int event)
{
    struct event *evt;
    volatile struct vic *vic;
    int irq;

    evt = &tab->events[event];
    if (evt->irq < 0)
        return EVT_NOT_REG;

    /* Disable that vectored interrupt, and the associated interrupt */
    irq = evt->irq;
    split_irq(&vic, &irq);
    intr_enable( vic, irq,   false);
    vintr_enable(vic, event, false);
    vintr_set(   vic, event, 0);
    vintr_setisr(vic, event, 0xdeadbeef);

    return 0;
}

int
evt_cur(void)
{
    uint32_t vec1, vec2;
    bool     got1, got2;

    got1 = vintr_cur(VIC1, &vec1);
    got2 = vintr_cur(VIC2, &vec2);

    assert(got1 || got2);
    if (!got1)
        return vec2;
    if (!got2)
        return vec1;

    if (vec1 < vec2) {
        vintr_clear(VIC2);
        return vec1;
    } else {
        vintr_clear(VIC1);
        return vec2;
    }
}

void
evt_clear(struct eventab *tab, int event)
{
    vintr_clear(tab->events[event].irq < 32 ? VIC1 : VIC2);
}

void
evt_enable(struct eventab *tab, int event, void *cbptr, size_t cbsize)
{
    volatile struct vic *vic;
    struct event *evt;
    int irq;

    evt = &tab->events[event];
    evt->ptr  = cbptr;
    evt->size = cbsize;

    irq = evt->irq;
    split_irq(&vic, &irq);
    intr_enable(vic, irq, true);
}

void
evt_disable(struct eventab *tab, int event)
{
    volatile struct vic *vic;
    int irq = tab->events[event].irq;
    split_irq(&vic, &irq);
    intr_enable(vic, irq, false);
}

void
evt_cleanup(void)
{
    intr_reset_all();
}

static void
split_irq(volatile struct vic **vic, int *irq)
{
    if (*irq < 32) {
        *vic = VIC1;
    } else {
        *vic = VIC2;
        *irq -= 32;
    }
}
