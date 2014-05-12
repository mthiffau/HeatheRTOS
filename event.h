#ifndef EVENT_H
#define EVENT_H

#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "u_tid.h"
#include "config.h"

#include "intr.h"

/* Error codes. */
enum {
    IRQ_OOR     = -1,
    IRQ_IN_USE  = -2,
    EVT_NOT_REG = -3,
    EVT_DBL_REG = -4
};

/* An event slot. Each registered event belongs to a particular task.
 * It is associated with a particular IRQ.
 * Events are triggered based on IRQs, and prioritized using the VIC.
 * Events have priority in order of their event ID. */
struct event {
    tid_t  tid;                 /* owning task */
    int  (*cb)(void*, size_t);  /* callback supplied by owning task */
    void  *ptr;                 /* AwaitEvent() arguments */
    size_t size;                /* (these are passed to cb) */
};

/* Event table. Accounts for all event registration data. */
struct eventab {
    struct event events[IRQ_COUNT];
};

/* Initialize the event table. This resets the interrupt controller. */
void evt_init(struct eventab *tab);

/* Register an event to a given task with a given callback function.
 * Returns 0 for success, or:
 *  - EVT_OOR if the event ID is out of range
 *  - EVT_IN_USE if the event ID is already in use
 *  - IRQ_OOR if the IRQ is out of range
 *  - IRQ_IN_USE if the IRQ is already in use
 */
int evt_register(
    struct eventab *tab,
    tid_t tid,
    int irq,
    int (*cb)(void*, size_t));

/* Unregister a registered event by ID. Returns 0 for success, or
 * EVT_NOT_REG if that event is not registered. */
int  evt_unregister(struct eventab *tab, int event);

/* Get the number of the next IRQ to handle. */
int  evt_cur(void);

/* Enable the IRQ corresponding to the given event.
 * This accepts parameters for AwaitEvent, which need to be saved
 * for the callback. */
void evt_enable(struct eventab *tab, int event, void*, size_t);

/* Disable the IRQ corresponding to the given event.
 * IRQs are kept disabled unless their owning task is inside AwaitEvent().
 * This way, it's impossible to swallow an IRQ while no task is waiting. */
void evt_disable(struct eventab *tab, int event);

/* Do any work needed for the interrupt controller before resuming a user
 task. This may be clearing the particular interrupt, etc. */
void evt_acknowledge(void);

/* Clean up interrupt controller state. */
void evt_cleanup(void);

#endif
