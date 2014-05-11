#ifdef VFP_ENABLE_H
#error "double-included vfp_enable.h"
#endif

#define VFP_ENABLE_H

TASK_H;

void vfp_cp_enable(void);
void vfp_cp_disable(void);
void vfp_enable(void);
void vfp_disable(void);

void vfp_save_state(volatile struct task_fpu_regs**, void* stack_pointer);
void vfp_load_state(volatile struct task_fpu_regs**);
void vfp_load_fresh(void);

