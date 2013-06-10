#ifdef EVENT_H
#error "double-included event.h"
#endif

#define EVENT_H

XBOOL_H;
XINT_H;
XDEF_H;
U_TID_H;
CONFIG_H;

#define IRQ_COUNT       64

/* Events have priority in order of their event ID. */

enum {
    EVT_OOR     = -1,
    EVT_IN_USE  = -2,
    IRQ_OOR     = -3,
    IRQ_IN_USE  = -4,
    EVT_NOT_REG = -5,
    EVT_DBL_REG = -6
};

struct event {
    int    irq;
    tid_t  tid;
    int  (*cb)(void*, size_t);
    void  *ptr;
    size_t size;
};

struct eventab {
    struct event events[IRQ_PRIORITIES];
    uint32_t     irq_used[IRQ_COUNT / 32 + (IRQ_COUNT % 32 ? 1 : 0)];
};

void evt_init(struct eventab *tab);

int evt_register(
    struct eventab *tab,
    tid_t tid,
    int event,
    int irq,
    int (*cb)(void*, size_t));

int  evt_unregister(struct eventab *tab, int event);
int  evt_cur(void);
void evt_clear(struct eventab *tab, int event);
void evt_enable(struct eventab *tab, int event, void*, size_t);
void evt_disable(struct eventab *tab, int event);
void evt_cleanup(void);
