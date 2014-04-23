#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "u_tid.h"
#include "config.h"
#include "event.h"

#include "intr.h"
#include "array_size.h"
#include "xassert.h"

void
evt_init(struct eventab *tab)
{
    unsigned i;
    intr_reset();
    for (i = 0; i < ARRAY_SIZE(tab->events); i++)
        tab->events[i].tid = -1;
}

int evt_register(
    struct eventab *tab,
    tid_t tid,
    int irq,
    int (*cb)(void*, size_t))
{
    struct event *evt;

    if (irq < 0 || irq >= IRQ_COUNT)
        return IRQ_OOR;

    evt = &tab->events[irq];
    if (evt->tid >= 0)
        return IRQ_IN_USE;

    /* Set up the event */
    evt->tid = tid;
    evt->cb  = cb;

    /* Set priority and ensure that it will be an IRQ,
       not a FIQ. */
    intr_config(irq, 0, false);
    return 0;
}

int
evt_unregister(struct eventab *tab, int irq)
{
    struct event *evt;

    evt = &tab->events[irq];
    if (evt->tid < 0)
        return EVT_NOT_REG;

    /* Disable the interrupt */
    intr_enable(irq, false);

    /* Free up the IRQ */
    evt->tid = -1;
    return 0;
}

int
evt_cur(void)
{
    int irq;
    irq = intr_cur();
    assert(irq >= 0);
    return irq;
}

void
evt_enable(struct eventab *tab, int irq, void *cbptr, size_t cbsize)
{
    struct event *evt;

    evt = &tab->events[irq];
    evt->ptr  = cbptr;
    evt->size = cbsize;

    intr_enable(irq, true);
}

void
evt_disable(struct eventab *tab, int irq)
{
    (void)tab;
    intr_enable(irq, false);
}

void
evt_cleanup(void)
{
    intr_reset();
}
