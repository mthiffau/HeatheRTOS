#ifdef INTR_H
#error "double-included intr.h"
#endif

#define INTR_H

XBOOL_H;
XINT_H;

struct vic;

#define VIC1 ((volatile struct vic*)0x800b0000)
#define VIC2 ((volatile struct vic*)0x800c0000)

/* Enable/disable a given interrupt. */
void intr_enable(volatile struct vic*, int intr, bool enable);

/* Set a given interrupt to be treated as FIQ or IRQ. */
void intr_setfiq(volatile struct vic*, int intr, bool fiq);

/* Forcibly assert/unassert a given interrupt from software. */
void intr_assert(volatile struct vic*, int intr, bool assert);

/* Forcibly assert/unassert a given set of interrupts from software. */
void intr_assert_mask(volatile struct vic*, uint32_t mask, bool assert);

/* Get the lowest-numbered asserted IRQ.
 * IRQs should be infrequent enough that ordering doesn't matter.
 * Returns -1 if no IRQs are asserted. */
int intr_cur(volatile struct vic*);

/* Reset all VIC state to default. */
void intr_reset_all(void);

/* Reset VIC state to default - all interrupts off, IRQ selected. */
void intr_reset(volatile struct vic*);

/* Print VIC debug info with busy-wait I/O */
void vic_dbgprint(volatile struct vic*);
