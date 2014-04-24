#ifdef INTR_H
#error "double-included intr.h"
#endif

#define INTR_H

XBOOL_H;
XINT_H;

/* Enable/disable a given interrupt. */
void intr_enable(int intr, bool enable);

/* Set a given interrupt to be treated as FIQ or IRQ. */
void intr_config(int intr, unsigned int prio, bool fiq);

/* Get the lowest-numbered asserted IRQ.
 * IRQs should be infrequent enough that ordering doesn't matter.
 * Returns -1 if no IRQs are asserted. */
int intr_cur();

/* Reset VIC state to default - all interrupts off, IRQ selected. */
void intr_reset(void);

/* Acknowledge IRQ and get ready to go again */
void intr_acknowledge(void);
