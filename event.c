/*******************************************************************************
    Copyright 2014 Matthew Thiffault

    This file is part of HeatheRTOS.

    HeatheRTOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    HeatheRTOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HeatheRTOS.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************/

#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "u_tid.h"
#include "config.h"
#include "event.h"

#include "intr.h"
#include "array_size.h"
#include "xassert.h"

/* Initialize the kernel IRQ event system */
void
evt_init(struct eventab *tab)
{
    unsigned i;
    /* Reset the interrupt controller */
    intr_reset();
    /* Reset the event table */
    for (i = 0; i < ARRAY_SIZE(tab->events); i++)
        tab->events[i].tid = -1;
}

/* Register a task to handle an IRQ */
int evt_register(
    struct eventab *tab,
    tid_t tid,
    int irq,
    int (*cb)(void*, size_t))
{
    struct event *evt;

    /* Check that the IRQ number makes sense */
    if (irq < 0 || irq >= IRQ_COUNT)
        return IRQ_OOR;

    /* Check that no other task has already registered */
    evt = &tab->events[irq];
    if (evt->tid >= 0)
        return IRQ_IN_USE;

    /* Set up the event */
    evt->tid = tid;
    evt->cb  = cb;

    /* Set priority and ensure that it will be an IRQ,
       not a FIQ. */
    intr_config(irq, 1, false);
    return 0;
}

/* Un-register a task with respect to an IRQ */
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

/* What is the current highest priority event
   to handle? */
int
evt_cur(void)
{
    int irq;
    irq = intr_cur();
    assert(irq >= 0);
    return irq;
}

/* Enable (unmask) a particular interrupt, providing the optional
   arguments to the handler callback */
void
evt_enable(struct eventab *tab, int irq, void *cbptr, size_t cbsize)
{
    struct event *evt;

    evt = &tab->events[irq];
    evt->ptr  = cbptr;
    evt->size = cbsize;

    intr_enable(irq, true);
}

/* Disable (mask) a particular interrupt */
void
evt_disable(struct eventab *tab, int irq)
{
    (void)tab;
    intr_enable(irq, false);
}

/* Acknowledge the IRQ so that more can happen */
void
evt_acknowledge(void)
{
    intr_acknowledge();
}

/* Cleanup/reset the interrupt controller */
void
evt_cleanup(void)
{
    intr_reset();
}
