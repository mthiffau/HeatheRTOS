#ifndef CPU_H
#define CPU_H

/*****************************************************************************
                           FUNCTION PROTOTYPES
*****************************************************************************/

/*
  Functions used internally
*/
extern void CPUirqd(void);
extern void CPUirqe(void);
extern void CPUfiqd(void);
extern void CPUfiqe(void);
extern unsigned int CPUIntStatus(void);

#endif /* CPU_H */
