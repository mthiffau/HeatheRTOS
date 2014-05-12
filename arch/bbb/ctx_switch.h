#ifndef CTX_SWITCH_H
#define CTX_SWITCH_H

#include "xint.h"
#include "task.h"

/* Kernel entry for software interrupts */
void     kern_entry_swi(void);
/* Kernel entry for IRQ handling */
void     kern_entry_irq(void);
/* Kernel entry for undefined instructions */
void     kern_entry_undef(void);
/* Context switch into a task, return trap reason */
uint32_t ctx_switch(struct task_desc *td);

#endif
