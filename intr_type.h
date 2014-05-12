#ifndef INTR_TYPE_H
#define INTR_TYPE_H

/* Types returned by ctx_switch,
   representing why the trap happened */
#define INTR_SWI       0x0
#define INTR_IRQ       0x1
#define INTR_UNDEF     0x2

#endif
