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

struct reservation {
    bool reserved;
    int train_id;
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
        tracksrv.reservations[i].reserved = false;
	tracksrv.reservations[i].train_id = -1;
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
    struct reservation *res, *rres;
    bool reserve_success;
    int  rc;

    res  = &TRACK_EDGE_DATA(track->track, edge, track->reservations);
    rres = &TRACK_EDGE_DATA(track->track, edge->reverse, track->reservations);

    reserve_success = false;
    if (!res->reserved) {
        assert(!rres->reserved);
        reserve_success = true;
    } else if (res->train_id == train) {
        assert(rres->reserved && rres->train_id == train);
        reserve_success = true;
    }

    if (!reserve_success) {
        dbglog(&track->dbglog,
            "train%d failed to get %s->%s: already owned by %d",
            train,
            edge->src->name,
            edge->dest->name,
            res->train_id);
    } else {
	res->reserved = true;
	res->train_id = train;
        rres->reserved = true;
        rres->train_id = train;
        dbglog(&track->dbglog,
            "train%d got %s->%s",
            train,
            edge->src->name,
            edge->dest->name);
    }

    rc = Reply(client, &reserve_success, sizeof(bool));
    assertv(rc, rc == 0);
}

static void
tracksrv_release(
    struct tracksrv *track,
    tid_t client,
    track_edge_t edge,
    int train)
{
    struct reservation *res, *rres;
    int rc;

    res  = &TRACK_EDGE_DATA(track->track, edge, track->reservations);
    rres = &TRACK_EDGE_DATA(track->track, edge->reverse, track->reservations);
    
    /* It should be reserved, and it should be us that reserved it */
    assert(res->reserved);
    assert(res->train_id == train);
    assert(rres->reserved);
    assert(rres->train_id == train);

    res->reserved  = false;
    res->train_id  = -1;
    rres->reserved = false;
    rres->train_id = -1;

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
    rc = Reply(client, &res->train_id, sizeof (res->train_id));
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

int
track_query(struct trackctx *ctx, track_edge_t edge)
{
    struct trackmsg msg;
    int who, rplylen;

    msg.type = TRACKMSG_QUERY;
    msg.train_id = ctx->train_id;
    msg.edge     = edge;

    rplylen = Send(ctx->tracksrv_tid, &msg, sizeof (msg), &who, sizeof (who));
    assertv(rplylen, rplylen == sizeof (who));

    return who;
}
