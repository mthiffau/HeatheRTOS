#ifdef SWITCH_SRV_H
#error "double-included switch_srv.h"
#endif

#define SWITCH_SRV_H

XBOOL_H;
XINT_H;
U_TID_H;

/* Switch server entry point. */
void switchsrv_main(void);

struct switchctx {
    tid_t switchsrv_tid;
};

/* Initialize switch server context */
void switchctx_init(struct switchctx *ctx);

/* Notify switch server that a turnout has been switched.
 * Only the train control multiplexer should call this. */
void switch_updated(struct switchctx *ctx, uint8_t sw, bool curved);

/* Wait for a turnout to switch.
 * Returns the switch whose state changed, and
 * sets *curved_out to true if it is curved, false otherwise.
 *
 * Only one client should ever call this function,
 * since it's assumed it knows the previous states
 * of all switches. */
uint8_t switch_wait(struct switchctx *ctx, bool *curved_out);
