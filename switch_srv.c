#include "xbool.h"
#include "xint.h"
#include "u_tid.h"
#include "switch_srv.h"

#include "xdef.h"
#include "array_size.h"
#include "xassert.h"

#include "u_syscall.h"
#include "ns.h"

#define MAX_SWITCHES    256

enum {
    SWMSG_UPDATED,
    SWMSG_WAIT,
    SWMSG_ISCURVED,
};

struct swmsg {
    int     type;
    /* for SWMSG_UPDATED only */
    uint8_t sw;
    bool    curved;
};

struct turnout {
    bool            curved;
    bool            dirty;
    struct turnout *next;
};

struct swsrv {
    tid_t           wait_client;
    struct turnout *dirty_head, *dirty_last;
    struct turnout  turnouts[MAX_SWITCHES];
};

void
switchsrv_main(void)
{
    struct swsrv swsrv;
    struct swmsg msg;
    struct turnout *turnout;
    tid_t client;
    int rc, msglen;
    unsigned i;

    /* Initialize */
    swsrv.wait_client = -1;
    swsrv.dirty_head  = NULL;
    swsrv.dirty_last  = NULL;
    for (i = 0; i < ARRAY_SIZE(swsrv.turnouts); i++)
        swsrv.turnouts[i].dirty = false;

    /* Register */
    rc = RegisterAs("switchsrv");
    assertv(rc, rc == 0);

    /* Event loop */
    for (;;) {
        /* Get message */
        msglen = Receive(&client, &msg, sizeof (msg));
        assertv(msglen, msglen == sizeof (msg));
        switch (msg.type) {
        case SWMSG_WAIT:
            swsrv.wait_client = client;
            break;
        case SWMSG_ISCURVED:
            rc = Reply(client, &swsrv.turnouts[msg.sw].curved, sizeof (bool));
            assertv(rc, rc == 0);
            break;
        case SWMSG_UPDATED:
            /* Immediately reply to update message */
            rc = Reply(client, NULL, 0);
            assertv(rc, rc == 0);
            /* Record turnout state and add it to dirty queue */
            turnout = &swsrv.turnouts[msg.sw];
            turnout->curved = msg.curved;
            if (turnout->dirty)
                break;
            turnout->dirty = true;
            turnout->next  = NULL;
            if (swsrv.dirty_last == NULL)
                swsrv.dirty_head = turnout;
            else
                swsrv.dirty_last->next = turnout;
            swsrv.dirty_last = turnout;
            break;
        default:
            panic("invalid switch server message type %d", msg.type);
        }

        /* Attempt to notify wait client */
        if (swsrv.wait_client == -1 || swsrv.dirty_head == NULL)
            continue;

        msg.type   = SWMSG_UPDATED;
        msg.sw     = swsrv.dirty_head - swsrv.turnouts;
        msg.curved = swsrv.dirty_head->curved;
        rc = Reply(swsrv.wait_client, &msg, sizeof (msg));
        assertv(rc, rc == 0);

        swsrv.dirty_head->dirty = false;
        swsrv.dirty_head = swsrv.dirty_head->next;
        if (swsrv.dirty_head == NULL)
            swsrv.dirty_last = NULL;
    }
}

void
switchctx_init(struct switchctx *ctx)
{
    ctx->switchsrv_tid = WhoIs("switchsrv");
}

void
switch_updated(struct switchctx *ctx, uint8_t sw, bool curved)
{
    struct swmsg msg;
    int rplylen;
    msg.type   = SWMSG_UPDATED;
    msg.sw     = sw;
    msg.curved = curved;
    rplylen = Send(ctx->switchsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

uint8_t
switch_wait(struct switchctx *ctx, bool *curved_out)
{
    struct swmsg msg;
    int rplylen;
    msg.type = SWMSG_WAIT;
    rplylen = Send(ctx->switchsrv_tid, &msg, sizeof (msg), &msg, sizeof (msg));
    assertv(rplylen, rplylen == sizeof (msg));
    assert(msg.type == SWMSG_UPDATED);
    *curved_out = msg.curved;
    return msg.sw;
}

bool
switch_iscurved(struct switchctx *ctx, uint8_t sw)
{
    struct swmsg msg;
    bool r;
    int rplylen;
    msg.type = SWMSG_ISCURVED;
    msg.sw   = sw;
    rplylen = Send(ctx->switchsrv_tid, &msg, sizeof (msg), &r, sizeof (r));
    assertv(rplylen, rplylen == sizeof (r));
    return r;
}
