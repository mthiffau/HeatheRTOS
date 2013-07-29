#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "u_tid.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "tracksel_srv.h"
#include "track_srv.h"

#include "array_size.h"
#include "xassert.h"

#include "u_syscall.h"
#include "ns.h"

#include "dbglog_srv.h"

enum {
    TRACKMSG_RESERVE,
    TRACKMSG_RELEASE,
    TRACKMSG_QUERY,
    TRACKMSG_DISABLE,
};

enum {
    TRACK_RESERVE_OK,
    TRACK_RESERVE_FAIL,
};

struct trackmsg {
    int          type;
    int          train_id;
    track_edge_t edge;
};

struct tracksrv {
    track_graph_t track;
    struct reservation reservations[TRACK_EDGES_MAX];
    struct dbglogctx   dbglog;
};

static void tracksrv_reserve(
    struct tracksrv *track, tid_t client, track_edge_t edge, int train);
static void tracksrv_release(
    struct tracksrv *track, tid_t client, track_edge_t edge, int train);
static void   tracksrv_query(
    struct tracksrv *track, tid_t client, track_edge_t edge);
static void   tracksrv_disable(
    struct tracksrv *track, tid_t client, track_edge_t edge);

void
tracksrv_main(void)
{
    struct tracksrv tracksrv;
    struct trackmsg msg;
    tid_t client;
    int rc, msglen;
    unsigned i;

    dbglogctx_init(&tracksrv.dbglog);

    for (i = 0; i < ARRAY_SIZE(tracksrv.reservations); i++) {
        tracksrv.reservations[i].state    = TRACK_FREE;
        tracksrv.reservations[i].disabled = false;
	tracksrv.reservations[i].train_id = -1;
	tracksrv.reservations[i].refcount = 0;
    }

    tracksrv.track = tracksel_ask();

    /* Register */
    rc = RegisterAs("tracksrv");
    assertv(rc, rc == 0);

    /* Event loop */
    for (;;) {
        /* Get message */
        msglen = Receive(&client, &msg, sizeof (msg));
        assertv(msglen, msglen == sizeof (msg));

        switch (msg.type) {
        case TRACKMSG_RESERVE:
	    tracksrv_reserve(&tracksrv, client, msg.edge, msg.train_id);
            break;
        case TRACKMSG_RELEASE:
	    tracksrv_release(&tracksrv, client, msg.edge, msg.train_id);
            break;
        case TRACKMSG_QUERY:
	    tracksrv_query(&tracksrv, client, msg.edge);
            break;
        case TRACKMSG_DISABLE:
            tracksrv_disable(&tracksrv, client, msg.edge);
            break;
        default:
            panic("invalid track server message type %d", msg.type);
        }
    }
}

static void 
tracksrv_reserve(
    struct tracksrv *track,
    tid_t client,
    track_edge_t edge,
    int train)
{
    struct reservation *res;
    bool reserve_success;
    int  rc;

    res = &TRACK_EDGE_DATA(track->track, edge, track->reservations);
    if (res->state == TRACK_RESERVED
        || res->disabled
        || (res->state == TRACK_BLOCKED && res->train_id != train)) {
        reserve_success = false;
        /* dbglog(&track->dbglog,
            "train%d failed to get %s->%s: already owned by %d",
            train,
            edge->src->name,
            edge->dest->name,
            res->train_id);*/
    } else {
        int i;
        track_edge_t subedge;
        reserve_success = true;
        for (i = 0; i < edge->mutex_len; i++) {
            subedge = edge->mutex[i];
            res     = &TRACK_EDGE_DATA(track->track, subedge, track->reservations);
            if (subedge == edge || subedge == edge->reverse)
                res->state = TRACK_RESERVED;
            else if (res->state == TRACK_FREE)
                res->state = TRACK_BLOCKED;
            res->train_id = train;
            res->refcount++;
        }
        dbglog(&track->dbglog,
            "train%d got %s->%s",
            train,
            edge->src->name,
            edge->dest->name);
    }

    rc = Reply(client, &reserve_success, sizeof (reserve_success));
    assertv(rc, rc == 0);
}

static void
tracksrv_release(
    struct tracksrv *track,
    tid_t client,
    track_edge_t edge,
    int train)
{
    struct reservation *res;
    track_edge_t subedge;
    int i, rc;

    res = &TRACK_EDGE_DATA(track->track, edge, track->reservations);
    assert(res->state == TRACK_RESERVED);
    for (i = 0; i < edge->mutex_len; i++) {
        subedge = edge->mutex[i];
        res     = &TRACK_EDGE_DATA(track->track, subedge, track->reservations);
        assert(res->train_id == train);
        if (--res->refcount == 0) {
            res->state = TRACK_FREE;
            res->train_id = -1;
        } else if (subedge == edge || subedge == edge->reverse) {
            res->state = TRACK_BLOCKED;
        }
    }

    dbglog(&track->dbglog,
        "train%d released %s->%s",
        train,
        edge->src->name,
        edge->dest->name);

    rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);
}

static void
tracksrv_query(
    struct tracksrv *track,
    tid_t client,
    track_edge_t edge)
{
    struct reservation *res;
    int  rc;

    res = &TRACK_EDGE_DATA(track->track, edge, track->reservations);
    rc = Reply(client, res, sizeof (*res));
    assertv(rc, rc == 0);
}

static void
tracksrv_disable(
    struct tracksrv *track,
    tid_t client,
    track_edge_t edge)
{
    struct reservation *res;
    int  rc;

    res = &TRACK_EDGE_DATA(track->track, edge, track->reservations);
    res->disabled = true;
    rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);
}

void
trackctx_init(struct trackctx *ctx, int train_id)
{
    ctx->tracksrv_tid = WhoIs("tracksrv");
    ctx->train_id = train_id;
}

bool
track_reserve(struct trackctx *ctx, track_edge_t edge)
{
    struct trackmsg msg;
    bool reserve_success;
    int rplylen;

    msg.type     = TRACKMSG_RESERVE;
    msg.train_id = ctx->train_id;
    msg.edge     = edge;

    rplylen = Send(ctx->tracksrv_tid, &msg, sizeof (msg), &reserve_success, sizeof(bool));
    assertv(rplylen, rplylen == sizeof (reserve_success));

    return reserve_success;
}

void
track_release(struct trackctx *ctx, track_edge_t edge)
{
    struct trackmsg msg;
    int rplylen;

    msg.type     = TRACKMSG_RELEASE;
    msg.train_id = ctx->train_id;
    msg.edge     = edge;

    rplylen = Send(ctx->tracksrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

void
track_query(
    struct trackctx *ctx,
    track_edge_t edge,
    struct reservation *res_out)
{
    struct trackmsg msg;
    int rplylen;

    msg.type = TRACKMSG_QUERY;
    msg.train_id = ctx->train_id;
    msg.edge     = edge;

    rplylen = Send(
        ctx->tracksrv_tid,
        &msg,
        sizeof (msg),
        res_out,
        sizeof (*res_out));
    assertv(rplylen, rplylen == sizeof (*res_out));
}

void
track_disable(struct trackctx *ctx, track_edge_t edge)
{
    struct trackmsg msg;
    int rplylen;

    msg.type = TRACKMSG_DISABLE;
    msg.train_id = ctx->train_id;
    msg.edge     = edge;

    rplylen = Send(ctx->tracksrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}
