#ifdef TRAIN_SRV_H
#error "double-included train_srv.h"
#endif

#define TRAIN_SRV_H

XINT_H;
TRACK_GRAPH_H;
U_TID_H;

void trainsrv_main(void);

struct trainctx {
    const struct track_graph *track;
    tid_t                     trainsrv_tid;
};

struct traincfg {
    uint8_t track_id;
    uint8_t train_id;
};

void trainctx_init(
    struct trainctx *ctx, const struct track_graph *track, uint8_t train_id);
void train_setspeed(struct trainctx *ctx, uint8_t speed);
void train_moveto(struct trainctx *ctx, const struct track_node *dest);
void train_stop(struct trainctx *ctx);
void train_vcalib(struct trainctx *ctx);
void train_stopcalib(struct trainctx *ctx, uint8_t speed);
void train_orient(struct trainctx *ctx);
