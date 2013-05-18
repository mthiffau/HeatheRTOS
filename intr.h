#ifdef INTR_H
#error "double-included intr.h"
#endif

#define INTR_H

#define INTR_SWI       0x0
#define INTR_SWI_FLAGS 0x00000000

#define INTR_IRQ       0x1
#define INTR_IRQ_FLAGS 0x10000000
