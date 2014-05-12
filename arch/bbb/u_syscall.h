#ifndef U_SYSCALL_H
#define U_SYSCALL_H

#include "xdef.h"
#include "u_tid.h"

tid_t Create(int priority, void (*task_entry)(void));
tid_t MyTid(void);
tid_t MyParentTid(void);
void  Pass(void);
void  Exit(void) __attribute__((noreturn));

int   Send(int TID, const void* msg, int msglen, void* reply, int replylen);
int   Receive(int* TID, void* msg, int msglen);
int   Reply(int TID, const void* reply, int replylen);

void  RegisterCleanup(void (*cleanup_cb)(void));

int   RegisterEvent(int irq, int (*cb)(void*, size_t));
int   AwaitEvent(void*, size_t);

/* Instruct the kernel to shut down immediately. */
void Shutdown(void) __attribute__((noreturn));
void Panic(const char *msg) __attribute__((noreturn));

#endif
