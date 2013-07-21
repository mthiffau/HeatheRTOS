#ifdef TRAIN_SRV_H
#error "double-included train_srv.h"
#endif

#define TRAIN_SRV_H

XINT_H;
TRACK_GRAPH_H;
TRACK_PT_H;
U_TID_H;

void trainsrv_main(void);

struct trainctx {
    tid_t trainsrv_tid;
};

struct traincfg {
    uint8_t train_id;
};

struct trainest {
    uint8_t         train_id;
    struct track_pt centre;
    track_node_t    lastsens;
    int             err_mm; /* position error at last sensor */
};

void trainctx_init(struct trainctx *ctx, uint8_t train_id);

/* Set desired cruising speed for the train */
void train_setspeed(struct trainctx *ctx, uint8_t speed);

/* Ask the train to move to a particular node. */
void train_moveto(struct trainctx *ctx, struct track_pt dest);

/* Stop the train. */
void train_stop(struct trainctx *ctx);

/* Wait for and get the next position estimate of the train. */
void train_estimate(struct trainctx *ctx, struct trainest *est_out);
