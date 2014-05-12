#ifndef IPC_H
#define IPC_H

#include "task.h"
#include "kern.h"

/* Immediate work for Send() system call. */
void ipc_send_start(struct kern *kern, struct task_desc *active);

/* Immediate work for Receive() system call. */
void ipc_receive_start(struct kern *kern, struct task_desc *active);

/* Immediate work for Reply() system call. */
void ipc_reply_start(struct kern *kern, struct task_desc *active);

#endif
