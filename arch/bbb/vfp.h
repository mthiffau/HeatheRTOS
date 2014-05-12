#ifndef VFP_ENABLE_H
#define VFP_ENABLE_H

#include "task.h"

void vfp_cp_enable(void);
void vfp_cp_disable(void);
void vfp_enable(void);
void vfp_disable(void);

void vfp_save_state(volatile struct task_fpu_regs**, void* stack_pointer);
void vfp_load_state(volatile struct task_fpu_regs**);
void vfp_load_fresh(void);

#endif
