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

#include "clock_srv.h"
#include "dbglog_srv.h"

enum {
    TRACKMSG_RESERVE,
    TRACKMSG_RELEASE,
    TRACKMSG_QUERY,
    TRACKMSG_DISABLE,
    TRACKMSG_SOFTRESERVE,
    TRACKMSG_SUBRESERVE,
    TRACKMSG_SUBRELEASE,
    TRACKMSG_DUMPSTATUS,
};

enum {
    TRACK_RESERVE_OK,
    TRACK_RESERVE_FAIL,
};

struct trackmsg {
    int          type;
    int          train_id;
    track_edge_t edge;
    int          clearance;
};

struct tracksrv {
    track_graph_t      track;
    struct reservation reservations[TRACK_EDGES_MAX];
    struct clkctx      clock;
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
static void   tracksrv_softreserve(
    struct tracksrv *track, tid_t client, track_edge_t edge, int train);
static void   tracksrv_subreserve(
    struct tracksrv *track,
    tid_t client,
    track_edge_t edge,
    int train,
    int clearance);
static void   tracksrv_subrelease(
    struct tracksrv *track,
    tid_t client,
    track_edge_t edge,
    int train,
    int clearance);
static void   tracksrv_dumpstatus(struct tracksrv *track, tid_t client);

void
tracksrv_main(void)
{
    struct tracksrv tracksrv;
    struct trackmsg msg;
    tid_t client;
    int rc, msglen;
    unsigned i;

    clkctx_init(&tracksrv.clock);
    dbglogctx_init(&tracksrv.dbglog);

    for (i = 0; i < ARRAY_SIZE(tracksrv.reservations); i++) {
        struct reservation *res = &tracksrv.reservations[i];
        res->state    = TRACK_FREE;
        res->disabled = false;
	res->train_id = -1;
	res->refcount = 0;
        res->sub_state = TRACK_FREE;
        res->sub_train_id = -1;
        res->sub_refcount = 0;
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
        case TRACKMSG_SOFTRESERVE:
            tracksrv_softreserve(&tracksrv, client, msg.edge, msg.train_id);
            break;
        case TRACKMSG_SUBRESERVE:
	    tracksrv_subreserve(
                &tracksrv,
                client,
                msg.edge,
                msg.train_id,
                msg.clearance);
            break;
        case TRACKMSG_SUBRELEASE:
	    tracksrv_subrelease(
                &tracksrv,
                client,
                msg.edge,
                msg.train_id,
                msg.clearance);
            break;
        case TRACKMSG_DUMPSTATUS:
            tracksrv_dumpstatus(&tracksrv, client);
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
    int reserve_code;
    int rc;

    res = &TRACK_EDGE_DATA(track->track, edge, track->reservations);
    if (res->state == TRACK_RESERVED
        || res->disabled
        || (res->state == TRACK_BLOCKED && res->train_id != train)) {
        reserve_code = RESERVE_FAILURE;
        /* dbglog(&track->dbglog,
            "train%d failed to get %s->%s: already owned by %d",
            train,
            edge->src->name,
            edge->dest->name,
            res->train_id);*/
    } else if (res->state == TRACK_SOFTRESERVED
        || res->state == TRACK_SOFTBLOCKED) {
        reserve_code = RESERVE_SOFTFAIL;
    } else {
        int i;
        track_edge_t subedge;
        reserve_code = RESERVE_SUCCESS;
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
        /*
        dbglog(&track->dbglog,
            "train%d got %s->%s",
            train,
            edge->src->name,
            edge->dest->name);
            */
    }

    rc = Reply(client, &reserve_code, sizeof (reserve_code));
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
    if (res->state != TRACK_RESERVED) {
        Delay(&track->clock, 100);
        panic("PANIC! can't release edge %s->%s (in state %d)",
            edge->src->name,
            edge->dest->name,
            res->state);
    }
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

    /*
    dbglog(&track->dbglog,
        "train%d released %s->%s",
        train,
        edge->src->name,
        edge->dest->name);
        */

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

static void
tracksrv_softreserve(
    struct tracksrv *track,
    tid_t client,
    track_edge_t edge,
    int train)
{
    struct reservation *res;
    track_edge_t subedge;
    int i, rc, refinc, subrefinc, now;

    res = &TRACK_EDGE_DATA(track->track, edge, track->reservations);
    assert(res->state == TRACK_FREE || res->train_id == train);
    assert(!res->disabled);

    rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);

    refinc = 1;
    if (res->state == TRACK_RESERVED)
        refinc = 0;

    subrefinc = 1 - refinc;

    now = Time(&track->clock);
    for (i = 0; i < edge->mutex_len; i++) {
        subedge = edge->mutex[i];
        res     = &TRACK_EDGE_DATA(track->track, subedge, track->reservations);
        if (res->train_id == train) {
            res->sub_state    = res->state;
            res->sub_train_id = train;
            res->state        = TRACK_FREE;
        } else {
            res->sub_state = TRACK_FREE;
        }
        if (subedge == edge || subedge == edge->reverse)
            res->state = TRACK_SOFTRESERVED;
        else if (res->state == TRACK_FREE)
            res->state = TRACK_SOFTBLOCKED;
        res->train_id = train;
        res->refcount     += refinc;
        res->sub_refcount += subrefinc;
        /* Not clear until owning train takes it and frees it again. */
        res->clear_until   = now - 1; 
    }
    /*
    dbglog(&track->dbglog,
        "train%d got %s->%s (soft)",
        train,
        edge->src->name,
        edge->dest->name);
    */
}

static void
tracksrv_subreserve(
    struct tracksrv *track,
    tid_t client,
    track_edge_t edge,
    int train,
    int clearance)
{
    struct reservation *res;
    bool success;
    int rc, now;

    res = &TRACK_EDGE_DATA(track->track, edge, track->reservations);
    assert(res->state == TRACK_SOFTRESERVED
        || res->state == TRACK_SOFTBLOCKED);
    assert(train != res->train_id || clearance == -1);

    now = Time(&track->clock);
    if (res->sub_state == TRACK_RESERVED
        || res->disabled
        || (res->sub_state == TRACK_BLOCKED && res->sub_train_id != train)) {
        success = false;
        /* dbglog(&track->dbglog,
            "train%d failed to get %s->%s: already owned by %d",
            train,
            edge->src->name,
            edge->dest->name,
            res->train_id);*/
    } else if (clearance >= 0 && res->clear_until - now < clearance) {
        success = false; /* Not enough time */
    } else {
        int i;
        track_edge_t subedge;
        success = true;
        for (i = 0; i < edge->mutex_len; i++) {
            subedge = edge->mutex[i];
            res     = &TRACK_EDGE_DATA(track->track, subedge, track->reservations);
            if (subedge == edge || subedge == edge->reverse)
                res->sub_state = TRACK_RESERVED;
            else if (res->sub_state == TRACK_FREE)
                res->sub_state = TRACK_BLOCKED;
            res->sub_train_id = train;
            res->sub_refcount++;
        }
        /*
        dbglog(&track->dbglog,
            "train%d got %s->%s (sub)",
            train,
            edge->src->name,
            edge->dest->name);
            */
    }

    assert(res->train_id != train || success);
    rc = Reply(client, &success, sizeof (success));
    assertv(rc, rc == 0);
}

static void
tracksrv_subrelease(
    struct tracksrv *track,
    tid_t client,
    track_edge_t edge,
    int train,
    int clearance)
{
    struct reservation *res;
    track_edge_t subedge;
    int i, rc, now;

    now = Time(&track->clock);

    res = &TRACK_EDGE_DATA(track->track, edge, track->reservations);
    assert(res->state == TRACK_SOFTRESERVED
        || res->state == TRACK_SOFTBLOCKED);
    assert(res->sub_state == TRACK_RESERVED);
    assert(train == res->train_id || clearance == -1);

    for (i = 0; i < edge->mutex_len; i++) {
        subedge = edge->mutex[i];
        res     = &TRACK_EDGE_DATA(track->track, subedge, track->reservations);
        assert(res->sub_train_id == train);
        if (--res->sub_refcount == 0) {
            res->sub_state    = TRACK_FREE;
            res->sub_train_id = -1;
        } else if (subedge == edge || subedge == edge->reverse) {
            res->sub_state = TRACK_BLOCKED;
        }
        if (clearance >= 0)
            res->clear_until = now + clearance;
    }

    /*
    dbglog(&track->dbglog,
        "train%d released %s->%s (sub)",
        train,
        edge->src->name,
        edge->dest->name);
    */

    rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);
}

static void
tracksrv_dumpstatus(struct tracksrv *track, tid_t client)
{
    static const char * const status_names[] = {
        "FREE",
        "RES",
        "BLK",
        "SRES",
        "SBLK"
    };
    int i, rc;
    rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);
    for (i = 0; i < track->track->n_nodes; i++) {
        int j, n_edges;
        track_node_t src = &track->track->nodes[i];
        n_edges = track_node_edges[src->type];
        for (j = 0; j < n_edges; j++) {
            track_edge_t edge = &src->edge[j];
            struct reservation *res;
            res = &TRACK_EDGE_DATA(track->track, edge, track->reservations);
            if (res->state == TRACK_FREE && !res->disabled)
                continue;
            dbglog(&track->dbglog, "%s->%s: T%d:%s [T%d:%s]",
                edge->src->name,
                edge->dest->name,
                res->train_id,
                status_names[res->state],
                res->sub_train_id,
                status_names[res->sub_state]);
        }
    }
}

void
trackctx_init(struct trackctx *ctx, int train_id)
{
    ctx->tracksrv_tid = WhoIs("tracksrv");
    ctx->train_id = train_id;
}

int
track_reserve(struct trackctx *ctx, track_edge_t edge)
{
    struct trackmsg msg;
    int rc;
    int rplylen;

    msg.type     = TRACKMSG_RESERVE;
    msg.train_id = ctx->train_id;
    msg.edge     = edge;

    rplylen = Send(ctx->tracksrv_tid, &msg, sizeof (msg), &rc, sizeof (rc));
    assertv(rplylen, rplylen == sizeof (rc));

    return rc;
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

void
track_softreserve(struct trackctx *ctx, track_edge_t edge)
{
    struct trackmsg msg;
    int rplylen;

    msg.type     = TRACKMSG_SOFTRESERVE;
    msg.train_id = ctx->train_id;
    msg.edge     = edge;

    rplylen = Send(ctx->tracksrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

bool
track_subreserve(struct trackctx *ctx, track_edge_t edge, int clearance)
{
    struct trackmsg msg;
    bool ok;
    int rplylen;

    msg.type     = TRACKMSG_SUBRESERVE;
    msg.train_id = ctx->train_id;
    msg.edge     = edge;
    msg.clearance= clearance;

    rplylen = Send(ctx->tracksrv_tid, &msg, sizeof (msg), &ok, sizeof (ok));
    assertv(rplylen, rplylen == sizeof (ok));

    return ok;
}

void
track_subrelease(struct trackctx *ctx, track_edge_t edge, int clearance)
{
    struct trackmsg msg;
    int rplylen;

    msg.type     = TRACKMSG_SUBRELEASE;
    msg.train_id = ctx->train_id;
    msg.edge     = edge;
    msg.clearance= clearance;

    rplylen = Send(ctx->tracksrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

void
track_dumpstatus(struct trackctx *ctx)
{
    struct trackmsg msg;
    int rplylen;

    msg.type     = TRACKMSG_DUMPSTATUS;
    msg.train_id = ctx->train_id;

    rplylen = Send(ctx->tracksrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}
