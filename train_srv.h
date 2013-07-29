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

struct respath {
    track_edge_t edges[32];
    int          earliest, next, count;
};

struct trainest {
    uint8_t         train_id;
    struct track_pt centre;
    track_node_t    lastsens;
    int             err_mm; /* position error at last sensor */
    struct track_pt dest;
    struct respath  respath;
};

void trainctx_init(struct trainctx *ctx, uint8_t train_id);

/* Set desired cruising speed for the train */
void train_set_speed(struct trainctx *ctx, uint8_t speed);

/* Set desired cruising speed for the train */
void train_set_rev_penalty_mm(struct trainctx *ctx, int penalty_mm);

/* Set desired cruising speed for the train */
void train_set_rev_slack_mm(struct trainctx *ctx, int slack_mm);

/* Set desired cruising speed for the train */
void train_set_init_rev_ok(struct trainctx *ctx, bool ok);

/* Set desired cruising speed for the train */
void train_set_rev_ok(struct trainctx *ctx, bool ok);

/* Ask the train to move to a particular node. */
void train_moveto(struct trainctx *ctx, struct track_pt dest);

/* Stop the train. */
void train_stop(struct trainctx *ctx);

/* Wait for and get the next position estimate of the train. */
void train_estimate(struct trainctx *ctx, struct trainest *est_out);

/* Instruct train to wander about. */
void train_wander(struct trainctx *ctx);

/* Instruct train to run the circuit. */
void train_circuit(struct trainctx *ctx);
