#ifdef _EXC_VEC_H_
#error "Double included exc_vec.h"
#endif

#define _EXC_VEC_H_

#define EXC_VEC_INSTR       0xe59ff018 /* ldr pc, [pc, #+0x18] */
#define EXC_VEC_SWI         ((unsigned int*)0x08)
#define EXC_VEC_IRQ         ((unsigned int*)0x18)
#define EXC_VEC_FP(i)       (*((void**)((void*)(i) + 0x20)))

