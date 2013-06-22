#ifdef SERIAL_SRV_H
#error "double-included serial_srv.h"
#endif

#define SERIAL_SRV_H

XBOOL_H;
XINT_H;
U_TID_H;

/* Return codes for Getc, Putc. */
enum {
    SERIAL_OK       =  0,
    SERIAL_OVERFLOW = -3, /* would overflow buffer */
    SERIAL_BUSY     = -4, /* line already in use */
};

/* Parameters for serial server. Must be the first message received by
 * serial server after it starts. */
struct serialcfg {
    uint8_t uart;
    bool    fifos;
    bool    nocts;
    int     baud;
    bool    parity;
    bool    parity_even;
    bool    stop2;
    uint8_t bits;
};

void serialsrv_main(void);

struct serialctx {
    tid_t serial_tid;
};

void serialctx_init(struct serialctx *ctx, int channel);
int Getc(struct serialctx *ctx);
int Putc(struct serialctx *ctx, char c);
