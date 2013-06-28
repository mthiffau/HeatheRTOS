#ifdef TCMUX_SRV_H
#error "double-included tcmux_srv.h"
#endif

#define TCMUX_SRV_H

XBOOL_H;
XINT_H;
U_TID_H;

/* Train control multiplexer server entry point. */
void tcmuxsrv_main(void);

struct tcmuxctx {
    tid_t tcmuxsrv_tid;
};

/* Get a context for communicating with the train control multiplexer. */
void tcmuxctx_init(struct tcmuxctx*);

/* Send a train speed command. */
void tcmux_train_speed(struct tcmuxctx*, uint8_t train, uint8_t speed);
void tcmux_train_speed_sync(struct tcmuxctx*, uint8_t train, uint8_t speed);

/* Send a switch command. */
void tcmux_switch_curve(struct tcmuxctx*, uint8_t sw, bool curved);
void tcmux_switch_curve_sync(struct tcmuxctx*, uint8_t sw, bool curved);

/* Enable or disable the lights on a train. */
void tcmux_train_light(struct tcmuxctx *tc, uint8_t train, bool enable);
void tcmux_train_light_sync(struct tcmuxctx *tc, uint8_t train, bool enable);
