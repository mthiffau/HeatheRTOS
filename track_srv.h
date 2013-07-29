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
    TRACK_FREE,     /* Unused */
    TRACK_RESERVED, /* Reserved by a train. */
    TRACK_BLOCKED,  /* Conflicts with an edge reserved by a train */
};

struct reservation {
    int  state;
    bool disabled;
    int  train_id;
    int  refcount;
};

/* Initialize track server context */
void trackctx_init(struct trackctx *ctx, int train_id);

/* Attempt to reserve a section of track */
bool track_reserve(struct trackctx *ctx, track_edge_t edge);

/* Release reservation on a section of track */
void track_release(struct trackctx *ctx, track_edge_t edge);

/* Check the current reservation status of a section of track. */
void track_query(
    struct trackctx *ctx, track_edge_t edge, struct reservation *res_out);

/* Disable an edge. */
void track_disable(struct trackctx *ctx, track_edge_t edge);
