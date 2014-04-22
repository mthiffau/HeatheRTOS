#ifdef _EXC_VEC_H_
#error "Double included exc_vec.h"
#endif

#define _EXC_VEC_H_

/* Copies the CPU exception vector table into memory */
void load_vector_table(void);
