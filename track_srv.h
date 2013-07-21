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

/* Initialize track server context */
void trackctx_init(struct trackctx *ctx, int train_id);

/* Attempt to reserve a section of track */
bool track_reserve(struct trackctx *ctx, track_edge_t edge);

/* Release reservation on a section of track */
void track_release(struct trackctx *ctx, track_edge_t edge);

/* Check the current reservation status of a section of track. */
int  track_query(struct trackctx *ctx, track_edge_t edge);
