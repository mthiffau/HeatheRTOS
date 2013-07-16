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
    int     type;
    int     train_id;
    int     edge_ix;
};

struct reservation {
    bool reserved;
    int train_id;
};

struct tracksrv {
    track_graph_t track;
    struct reservation reservations[TRACK_EDGES_MAX];
};

static void tracksrv_reserve(struct tracksrv *track, tid_t client, int edge_ix, int train);
static void tracksrv_release(struct tracksrv *track, tid_t client, int edge_ix, int train);
static void   tracksrv_query(struct tracksrv *track, tid_t client, int edge_ix);

void
tracksrv_main(void)
{
    struct tracksrv tracksrv;
    struct trackmsg msg;
    tid_t client;
    int rc, msglen;
    unsigned i;

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
	    tracksrv_reserve(&tracksrv, client, msg.edge_ix, msg.train_id);
            break;
        case TRACKMSG_RELEASE:
	    tracksrv_release(&tracksrv, client, msg.edge_ix, msg.train_id);
            break;
        case TRACKMSG_QUERY:
	    tracksrv_query(&tracksrv, client, msg.edge_ix);
            break;
        default:
            panic("invalid track server message type %d", msg.type);
        }
    }
}

static void 
tracksrv_reserve(struct tracksrv *track, tid_t client, int edge_ix, int train)
{
    bool is_reserved, reserve_success;
    int  reserver, rc;

    is_reserved = track->reservations[edge_ix].reserved;
    reserver    = track->reservations[edge_ix].train_id;

    if (!is_reserved || (is_reserved && (reserver == train))) {
	reserve_success = true;
	track->reservations[edge_ix].reserved = true;
	track->reservations[edge_ix].train_id = train;
    } else
	reserve_success = false;

    rc = Reply(client, &reserve_success, sizeof(bool));
    assertv(rc, rc == 0);
}

static void
tracksrv_release(struct tracksrv *track, tid_t client, int edge_ix, int train)
{
    bool is_reserved;
    int  reserver, rc;

    is_reserved = track->reservations[edge_ix].reserved;
    reserver    = track->reservations[edge_ix].train_id;
    
    /* It should be reserved, and it should be us that reserved it */
    assert(is_reserved);
    assert(reserver == train);

    track->reservations[edge_ix].reserved = false;
    track->reservations[edge_ix].train_id = -1;

    rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);
}

static void
tracksrv_query(struct tracksrv *track, tid_t client, int edge_ix)
{
    bool is_reserved;
    int  rc;

    is_reserved = track->reservations[edge_ix].reserved;

    rc = Reply(client, &is_reserved, sizeof(bool));
    assertv(rc, rc == 0);
}

void
trackctx_init(struct trackctx *ctx, track_graph_t track, int train_id)
{
    ctx->tracksrv_tid = WhoIs("tracksrv");
    ctx->track = track;
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
    msg.edge_ix  = ((edge->src - ctx->track->nodes) << 1) | (edge - edge->src->edge);

    rplylen = Send(ctx->tracksrv_tid, &msg, sizeof (msg), &reserve_success, sizeof(bool));
    assertv(rplylen, rplylen == 0);

    return reserve_success;
}

void
track_release(struct trackctx *ctx, track_edge_t edge)
{
    struct trackmsg msg;
    int rplylen;

    msg.type     = TRACKMSG_RELEASE;
    msg.train_id = ctx->train_id;
    msg.edge_ix  = ((edge->src - ctx->track->nodes) << 1) | (edge - edge->src->edge);

    rplylen = Send(ctx->tracksrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

bool
track_isreserved(struct trackctx *ctx, track_edge_t edge)
{
    struct trackmsg msg;
    bool is_reserved;
    int rplylen;

    msg.type = TRACKMSG_QUERY;
    msg.train_id = ctx->train_id;
    msg.edge_ix  = ((edge->src - ctx->track->nodes) << 1) | (edge - edge->src->edge);

    rplylen = Send(ctx->tracksrv_tid, &msg, sizeof (msg), &is_reserved, sizeof (bool));
    assertv(rplylen, rplylen == sizeof (bool));

    return is_reserved;
}
