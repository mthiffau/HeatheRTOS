#ifdef IPC_H
#error "double-included ipc.h"
#endif

#define IPC_H

TASK_H;
KERN_H;

/* Immediate work for Send() system call. */
void ipc_send_start(struct kern *kern, struct task_desc *active);

/* Immediate work for Receive() system call. */
void ipc_receive_start(struct kern *kern, struct task_desc *active);

/* Immediate work for Reply() system call. */
void ipc_reply_start(struct kern *kern, struct task_desc *active);
