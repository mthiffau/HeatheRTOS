#ifdef INTR_H
#error "double-included intr.h"
#endif

#define INTR_H

XBOOL_H;

struct vic;

#define VIC1 ((volatile struct vic*)0x800b0000)
#define VIC2 ((volatile struct vic*)0x800c0000)

/* Enable/disable a given interrupt. */
void intr_enable(volatile struct vic*, int intr, bool enable);

/* Set a given interrupt to be treated as FIQ or IRQ. */
void intr_setfiq(volatile struct vic*, int intr, bool fiq);

/* Reset all VIC state to default. */
void intr_reset_all(void);

/* Reset VIC state to default - all interrupts off, IRQ selected. */
void intr_reset(volatile struct vic*);

/* Print VIC debug info with busy-wait I/O */
void vic_dbgprint(volatile struct vic*);
