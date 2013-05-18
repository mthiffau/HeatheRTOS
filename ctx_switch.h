#ifdef CTX_SWITCH_H
#error "double-included ctx_switch.h"
#endif

#define CTX_SWITCH_H

XINT_H;
TASK_H;

void     trigger_swi(void); /* FIXME put me where stub functions go */
void     kern_entry_swi(void);
uint32_t ctx_switch(struct task_desc *td);
