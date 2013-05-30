#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "kern.h"
#include "ipc.h"

#include "xassert.h"
#include "xmemcpy.h"

#include "xarg.h"
#include "bwio.h"

#define SEND_ARG_TID(td)     ((tid_t)(td)->regs->r0)
#define SEND_ARG_MSG(td)     ((const char*)(td)->regs->r1)
#define SEND_ARG_MSGLEN(td)  ((int)(td)->regs->r2)
#define SEND_ARG_RPLY(td)    ((char*)(td)->regs->r3)
#define SEND_ARG_RPLYLEN(td) (*((int*)(td)->regs->sp))

#define RECV_ARG_PTID(td)    ((tid_t*)(td)->regs->r0)
#define RECV_ARG_MSG(td)     ((char*)(td)->regs->r1)
#define RECV_ARG_MSGLEN(td)  ((int)(td)->regs->r2)

#define RPLY_ARG_TID(td)     ((tid_t)(td)->regs->r0)
#define RPLY_ARG_RPLY(td)    ((const char*)(td)->regs->r1)
#define RPLY_ARG_RPLYLEN(td) ((int)(td)->regs->r2)

static void rendezvous(struct kern*, struct task_desc*, struct task_desc*);

void
ipc_send_start(struct kern *kern, struct task_desc *active)
{
    struct task_desc *srv;
    int rc;

    rc = get_task(kern, SEND_ARG_TID(active), &srv);
    if (rc != GET_TASK_SUCCESS) {
        active->regs->r0 = rc;
        task_ready(kern, active);
        return;
    }

    if (TASK_STATE(srv) == TASK_STATE_SEND_BLOCKED) {
        rendezvous(kern, active, srv);
    } else {
        TASK_SET_STATE(srv, TASK_STATE_RECEIVE_BLOCKED);
        task_enqueue(kern, active, &srv->senders);
    }
}

void
ipc_receive_start(struct kern *kern, struct task_desc *active)
{
    struct task_desc *sender = task_dequeue(kern, &active->senders);
    if (sender != NULL) {
        rendezvous(kern, sender, active);
    } else {
        TASK_SET_STATE(active, TASK_STATE_SEND_BLOCKED);
    }
}

void
ipc_reply_start(struct kern *kern, struct task_desc *active)
{
    bwputstr(COM2, "ipc_reply_start\n");
    bwprintf(COM2, "  tid     = %d\n",   RPLY_ARG_TID(active));
    bwprintf(COM2, "  rply    = 0x%x\n", RPLY_ARG_RPLY(active));
    bwprintf(COM2, "  rplylen = %d\n",   RPLY_ARG_RPLYLEN(active));
    task_ready(kern, active);
}

static void
rendezvous(
    struct kern *kern,
    struct task_desc *sender,
    struct task_desc *receiver)
{
    const char *send_msg;
    char *recv_msg;
    int send_msglen, recv_msglen, copy_msglen;

    send_msg    = SEND_ARG_MSG(sender);
    send_msglen = SEND_ARG_MSGLEN(sender);
    recv_msg    = RECV_ARG_MSG(receiver);
    recv_msglen = RECV_ARG_MSGLEN(receiver);
    copy_msglen = (recv_msglen < send_msglen) ? recv_msglen : send_msglen;

    /* Prepare for returning from Receive() */
    xmemcpy(recv_msg, send_msg, copy_msglen);
    *RECV_ARG_PTID(receiver) = TASK_TID(kern, sender);
    receiver->regs->r0 = send_msglen;

    /*TASK_SET_STATE(sender, TASK_STATE_REPLY_BLOCKED); TODO */
    task_ready(kern, sender); /* FIXME FIXME FIXME FIXME */
    task_ready(kern, receiver);
}
