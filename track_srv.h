#ifdef TRACK_SRV_H
#error "double-included track_srv.h"
#endif

#define TRACK_SRV_H

XBOOL_H;
XINT_H;
U_TID_H;
TRACK_GRAPH_H;

/* Track server entry point. */
void tracksrv_main(void);

struct trackctx {
    tid_t         tracksrv_tid;
    int           train_id;
};

enum {
    TRACK_FREE,         /* Unused */
    TRACK_RESERVED,     /* Reserved by a train. */
    TRACK_BLOCKED,      /* Conflicts with an edge reserved by a train */
    TRACK_SOFTRESERVED, /* Owned, but may be available for a short time. */
    TRACK_SOFTBLOCKED,  /* Conflicts with an edge that is soft-reserved. */
};

struct reservation {
    int  state;
    bool disabled;
    int  train_id;
    int  refcount;
    /* Temporary reservation of soft-reserved and soft-blocked edges. */
    int  sub_state;
    int  sub_train_id;
    int  sub_refcount;
    /* How long before owning train needs soft reservation again */
    int  clear_until;
};

/* Initialize track server context */
void trackctx_init(struct trackctx *ctx, int train_id);

/* Attempt to reserve a section of track */
int track_reserve(struct trackctx *ctx, track_edge_t edge);
enum {
    RESERVE_SUCCESS  =  0, /* Successfully reserved the edge */
    RESERVE_FAILURE  = -1, /* Edge not available. */
    RESERVE_SOFTFAIL = -2, /* Try making a sub-reservation. */
};

/* Release reservation on a section of track */
void track_release(struct trackctx *ctx, track_edge_t edge);

/* Check the current reservation status of a section of track. */
void track_query(
    struct trackctx *ctx, track_edge_t edge, struct reservation *res_out);

/* Disable an edge. */
void track_disable(struct trackctx *ctx, track_edge_t edge);

/* Soft-reserve an edge. The edge MUST be available. */
void track_softreserve(struct trackctx *ctx, track_edge_t edge);

/* Sub-reserve a soft-reserved edge. The edge MUST be soft-reserved.
 * For trains that don't own the soft reservation, clearance is the
 * number of ticks required by the train to get through. For the train
 * that owns the soft-reservation, clearance should be -1. */
bool track_subreserve(struct trackctx *ctx, track_edge_t edge, int clearance);

/* Sub-release a sub-reserved edge. The edge MUST be sub-reserved by the
 * requesting train. For trains that don't own the soft reservation,
 * clearance should be -1. For the train that owns the soft reservation,
 * clearance gives the number of ticks for which the edge is unneeded. */
void track_subrelease(struct trackctx *ctx, track_edge_t edge, int clearance);

/* Dump status to log. */
void track_dumpstatus(struct trackctx *ctx);
