#ifdef CTX_SWITCH_H
#error "double-included ctx_switch.h"
#endif

#define CTX_SWITCH_H

XINT_H;
TASK_H;

void     kern_entry_swi(void);
void     kern_entry_irq(void);
void     kern_entry_undef(void);
uint32_t ctx_switch(struct task_desc *td);
