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

struct trainest {
    uint8_t                  train_id;
    const struct track_edge *ahead;
    int                      ahead_mm;
    const struct track_node *lastsens;
    int                      err_mm; /* position error at last sensor */
};

void trainctx_init(
    struct trainctx *ctx, const struct track_graph *track, uint8_t train_id);

/* Set desired cruising speed for the train */
void train_setspeed(struct trainctx *ctx, uint8_t speed);

/* Ask the train to move to a particular node. */
void train_moveto(struct trainctx *ctx, const struct track_node *dest);

/* Stop the train. */
void train_stop(struct trainctx *ctx);

/* Calibrate the train's velocity. Output on debug log. */
void train_vcalib(struct trainctx *ctx);

/* Calibrate the train's stopping distance. */
void train_stopcalib(struct trainctx *ctx, uint8_t speed);

/* Have the train find itself after becoming disoriented. */
void train_orient(struct trainctx *ctx);

/* Wait for and get the next position estimate of the train. */
void train_estimate(struct trainctx *ctx, struct trainest *est_out);
