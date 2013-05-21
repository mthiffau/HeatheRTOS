#ifdef U_SYSCALL_H
#error "double-included u_syscall.h"
#endif

#define U_SYSCALL_H

U_TID_H;

tid_t Create(int priority, void (*task_entry)(void));
tid_t MyTid(void);
tid_t MyParentTid(void);
void  Pass(void);
void  Exit(void);
