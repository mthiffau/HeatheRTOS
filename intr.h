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

/* Associate a given vectored interrupt slot with a given interrupt. */
void vintr_set(volatile struct vic *vic, int vintr, int intr);

/* Enable/disable a given vectored interrupt slot. */
void vintr_enable(volatile struct vic *vic, int vintr, bool enable);

/* Set the default ISR address (for non-vectored interrupts?). */
void vintr_setdefisr(volatile struct vic *vic, uint32_t isr);

/* Set the ISR address for a given vectored interrupt slot. */
void vintr_setisr(volatile struct vic *vic, int vintr, uint32_t isr);

/* Get the ISR address for the current interrupt. */
uint32_t vintr_cur(volatile struct vic *vic);

/* Clear the current interrupt to allow lower-priority interrupts. */
void vintr_clear(volatile struct vic *vic);

/* Reset all VIC state to default. */
void intr_reset_all(void);

/* Reset VIC state to default - all interrupts off, IRQ selected. */
void intr_reset(volatile struct vic*);

/* Print VIC debug info with busy-wait I/O */
void vic_dbgprint(volatile struct vic*);
