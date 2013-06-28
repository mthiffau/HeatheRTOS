#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "u_tid.h"
#include "config.h"
#include "event.h"

#include "intr.h"
#include "array_size.h"
#include "xassert.h"

static void split_irq(volatile struct vic**, int*);

void
evt_init(struct eventab *tab)
{
    unsigned i;
    intr_reset_all();
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
    volatile struct vic *vic;

    if (irq < 0 || irq >= IRQ_COUNT)
        return IRQ_OOR;

    evt = &tab->events[irq];
    if (evt->tid >= 0)
        return IRQ_IN_USE;

    /* Set up the event */
    evt->tid = tid;
    evt->cb  = cb;

    /* Ensure that it will be an IRQ, not a FIQ. */
    split_irq(&vic, &irq);
    intr_setfiq(vic, irq, false);
    return 0;
}

int
evt_unregister(struct eventab *tab, int irq)
{
    struct event *evt;
    volatile struct vic *vic;

    evt = &tab->events[irq];
    if (evt->tid < 0)
        return EVT_NOT_REG;

    /* Disable the interrupt */
    split_irq(&vic, &irq);
    intr_enable(vic, irq, false);

    /* Free up the IRQ */
    evt->tid = -1;
    return 0;
}

int
evt_cur(void)
{
    int irq;
    irq = intr_cur(VIC1);
    if (irq >= 0)
        return irq;

    irq = intr_cur(VIC2);
    assert(irq >= 0);
    return irq + 32;
}

void
evt_enable(struct eventab *tab, int irq, void *cbptr, size_t cbsize)
{
    volatile struct vic *vic;
    struct event *evt;

    evt = &tab->events[irq];
    evt->ptr  = cbptr;
    evt->size = cbsize;

    split_irq(&vic, &irq);
    intr_enable(vic, irq, true);
}

void
evt_disable(struct eventab *tab, int irq)
{
    volatile struct vic *vic;
    (void)tab;
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
