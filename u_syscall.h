#ifdef U_SYSCALL_H
#error "double-included u_syscall.h"
#endif

#define U_SYSCALL_H

U_TID_H;

tid_t Create(int priority, void (*task_entry)(void));
tid_t MyTid(void);
tid_t MyParentTid(void);
void  Pass(void);
void  Exit(void) __attribute__((noreturn));

int   Send(int TID, const void* msg, int msglen, void* reply, int replylen);
int   Receive(int* TID, void* msg, int msglen);
int   Reply(int TID, const void* reply, int replylen);

/* Register with the name server under the given name.
 *
 * The name must fit into NS_NAME_MAXLEN characters,
 * including the trailing NUL. */
int RegisterAs(const char *name);

/* Find the TID of the task registered with the given name.
 *
 * If there is no task registered with that name, then WhoIs blocks
 * until such a task becomes registered.
 *
 * The name must fit into NS_NAME_MAXLEN characters,
 * including the trailing NUL. */
tid_t WhoIs(const char *name);
