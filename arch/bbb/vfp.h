#ifdef VFP_ENABLE_H
#error "double-included vfp_enable.h"
#endif

#define VFP_ENABLE_H

void vfp_cp_enable(void);
void vfp_cp_disable(void);
void vfp_enable(void);
void vfp_disable(void);

uint32_t vfp_save_state(uint32_t stack_ptr);
uint32_t vfp_load_state(uint32_t stack_ptr);
void vfp_load_fresh(void);
