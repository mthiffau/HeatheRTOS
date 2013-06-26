#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "u_tid.h"
#include "serial_srv.h"

#include "config.h"
#include "xarg.h"
#include "xstring.h"
#include "ringbuf.h"
#include "ts7200.h"
#include "bwio.h"
#include "timer.h"

#include "xassert.h"
#include "u_syscall.h"
#include "u_events.h"
#include "ns.h"
#include "array_size.h"
#include "static_assert.h"
#include "xmemcpy.h"

#ifdef SERIAL_STATS
#include "stats.h"
static struct stats *tx_delay_stats[2];
#endif

#define WRITE_MAXLEN  32
#define GETC_BUF_SIZE 512
#define PUTC_BUF_SIZE 512

#define NOTIFY_PRIO_RX          0
#define NOTIFY_PRIO_RXTO        1
#define NOTIFY_PRIO_TXR         2
#define NOTIFY_PRIO_GEN         3
#define NOTIFY_PRIO(uart, n)    (1 + (uart) * 4 + (n))

struct uart {
    uint32_t data;
    uint32_t rsr;
    uint32_t lcrh;
    uint32_t lcrm;
    uint32_t lcrl;
    uint32_t ctrl;
    uint32_t flag;
    uint32_t intr;
    uint32_t reserved[2];
    uint32_t dmar;
};

STATIC_ASSERT(data_offs, offsetof (struct uart, data) == UART_DATA_OFFSET);
STATIC_ASSERT(rsr_offs,  offsetof (struct uart, rsr)  == UART_RSR_OFFSET);
STATIC_ASSERT(lcrh_offs, offsetof (struct uart, lcrh) == UART_LCRH_OFFSET);
STATIC_ASSERT(lcrm_offs, offsetof (struct uart, lcrm) == UART_LCRM_OFFSET);
STATIC_ASSERT(lcrl_offs, offsetof (struct uart, lcrl) == UART_LCRL_OFFSET);
STATIC_ASSERT(ctrl_offs, offsetof (struct uart, ctrl) == UART_CTLR_OFFSET);
STATIC_ASSERT(flag_offs, offsetof (struct uart, flag) == UART_FLAG_OFFSET);
STATIC_ASSERT(intr_offs, offsetof (struct uart, intr) == UART_INTR_OFFSET);
STATIC_ASSERT(dmar_offs, offsetof (struct uart, dmar) == UART_DMAR_OFFSET);

enum {
    SERIALMSG_RX,    /* Received a character    empty reply */
    SERIALMSG_TXR,   /* Tx Ready                empty reply */
    SERIALMSG_CTS,   /* CTS status              empty reply */
    SERIALMSG_GETC,  /* Getc request            reply is Getc return value */
    SERIALMSG_PUTC,  /* Putc request            reply is Putc return value */
    SERIALMSG_FLUSH, /* Flush request           empty reply */
    SERIALMSG_WRITE, /* Write request           reply is integer code */
};

struct serialmsg {
    int type;
    union {
        uint8_t rx_data;
        bool    cts_status;
        uint8_t putc_arg;
        struct {
            int     len;
            uint8_t buf[WRITE_MAXLEN];
        } write;
#ifdef SERIAL_STATS
        int32_t txr_time;
#endif
    };
};

struct serialsrv {
    /* Serial server parameters */
    volatile struct uart *uart;
    bool                  fifos;
    bool                  nocts;

    /* Transmit state */
    bool           txr;
    bool           cts;
    struct ringbuf putc_buf;
    tid_t          flush_client;

    /* Receive state */
    tid_t          getc_client; /* negative if none */
    struct ringbuf getc_buf;

    /* Buffer memory */
    char getc_buf_mem[GETC_BUF_SIZE];
    char putc_buf_mem[PUTC_BUF_SIZE];

#ifdef SERIAL_STATS
    int32_t        txr_intr_time;
    struct stats   tx_delay_stats;
#endif
};

static void serialsrv_init(struct serialsrv *srv, struct serialcfg *cfg);
static void serialsrv_cleanup1(void);
static void serialsrv_cleanup2(void);
static void serialsrv_cleanup(int port);
static void serialsrv_uart_setup(struct serialcfg *cfg, 
                                 bool intrs, 
                                 volatile struct uart *uart);

static void serial_writechar(struct serialsrv *srv, char c);
static void serial_all_ready(struct serialsrv *srv);
static void serial_rx(struct serialsrv *srv, tid_t client, int rx_data);
static void serial_txr(struct serialsrv *srv, tid_t notifier);
static void serial_cts(struct serialsrv *srv, tid_t notifier, bool cts);
static void serial_putc(struct serialsrv *srv, tid_t client, char c);
static void serial_write(
    struct serialsrv *srv, tid_t client, uint8_t *buf, int len);
static void serial_getc(struct serialsrv *srv, tid_t client);
static void serial_flush(struct serialsrv *srv, tid_t client);

static tid_t
serial_notif_init(
    int evts[2],
    int intrs[2],
    int (*cb)(void*,size_t),
    struct serialcfg *cfg);

static void serialrx_notif(void);
static int  serialrx_notif_cb(void*, size_t);

static void serialtx_notif(void);
static int  serialtx_notif_cb(void*, size_t);

/* This notifier is for the rest of the
 UART interrupts which aren't indivually
 registerable (like modem status) */
static void serialgen_notif(void);
static int  serialgen_notif_cb(void*, size_t);

static volatile struct uart *get_uart(int which);

static void uart_ctrl_delay(void);

void
serialsrv_main(void)
{
    char name[8] = "serial\0";
    tid_t client;
    int rc, msglen;

    struct serialcfg cfg;
    struct serialsrv srv;
    struct serialmsg msg;

    /* Receive initial config. */
    msglen = Receive(&client, &cfg, sizeof (cfg));
    assertv(msglen, msglen == sizeof (cfg));
    assert(cfg.uart == 0 || cfg.uart == 1);
    assert(!cfg.fifos || cfg.nocts);
    assert(!cfg.fifos); /* FIXME */
    rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);

    /* Initialize serial server state (also starts notifiers). */
    serialsrv_init(&srv, &cfg);

    /* Register with name server. */
    name[sizeof (name) - 2] = '1' + cfg.uart;
    rc = RegisterAs(name);
    assertv(rc, rc == 0);

    /* Enter message-handling loop */
    for (;;) {
        msglen = Receive(&client, &msg, sizeof(msg));
        assertv(msglen, msglen == sizeof(msg));

        switch(msg.type) {
        case SERIALMSG_RX:
            serial_rx(&srv, client, msg.rx_data);
            break;
        case SERIALMSG_TXR:
#ifdef SERIAL_STATS
            {
                char c;
                if (rbuf_peekc(&srv.putc_buf, &c))
                    srv.txr_intr_time = msg.txr_time;
                else
                    srv.txr_intr_time = -1;
            }
#endif
            serial_txr(&srv, client);
            break;
        case SERIALMSG_CTS:
            serial_cts(&srv, client, msg.cts_status);
            break;
        case SERIALMSG_GETC:
            /* Char input request from a task */
            serial_getc(&srv, client);
            break;
        case SERIALMSG_PUTC:
            serial_putc(&srv, client, msg.putc_arg);
            break;
        case SERIALMSG_WRITE:
            serial_write(&srv, client, msg.write.buf, msg.write.len);
            break;
        case SERIALMSG_FLUSH:
            serial_flush(&srv, client);
            break;
        default:
            panic("unrecognized serial server message %d", msg.type);
        }
    }
}

static void
serialsrv_init(struct serialsrv *srv, struct serialcfg *cfg)
{
    static void (*cleanups[2])(void) = {
        &serialsrv_cleanup1,
        &serialsrv_cleanup2
    };
    int rc;
    tid_t tid;

    assert(cfg->bits >= 5 && cfg->bits <= 8);

    srv->uart  = get_uart(cfg->uart);
    srv->fifos = cfg->fifos;
    srv->nocts = cfg->nocts;

    RegisterCleanup(cleanups[cfg->uart]);

    serialsrv_uart_setup(cfg, true, srv->uart);

    /* Initialize getc state */
    srv->getc_client = -1;
    rbuf_init(&srv->getc_buf, srv->getc_buf_mem, sizeof (srv->getc_buf_mem));

    /* Initialize putc state */
    uart_ctrl_delay();
    srv->txr = false; /* interrupt will notify us, even if immediately */
    srv->cts = srv->nocts || (bool)(srv->uart->flag & CTS_MASK);
    rbuf_init(&srv->putc_buf, srv->putc_buf_mem, sizeof (srv->putc_buf_mem));
    srv->flush_client = -1;

    /* Initialize stats */
#ifdef SERIAL_STATS
    stats_init(&srv->tx_delay_stats);
    tx_delay_stats[cfg->uart] = &srv->tx_delay_stats;
#endif

    /* Start notifiers */
    tid = Create(PRIORITY_MAX, &serialrx_notif);
    assertv(tid, tid >= 0);
    rc = Send(tid, cfg, sizeof (*cfg), NULL, 0);
    assertv(rc, rc == 0);

    tid = Create(PRIORITY_MAX, &serialtx_notif);
    assertv(tid, tid >= 0);
    rc = Send(tid, cfg, sizeof (*cfg), NULL, 0);
    assertv(rc, rc == 0);

    if (!cfg->nocts) {
        tid = Create(PRIORITY_MAX, &serialgen_notif);
        assertv(tid, tid >= 0);
        rc = Send(tid, cfg, sizeof (*cfg), NULL, 0);
        assertv(rc, rc == 0);
    }
}

static void
serialsrv_uart_setup(struct serialcfg* cfg, bool intrs, volatile struct uart *uart)
{
    uint32_t ctrl;

    /* Set baud rate */
    uart_ctrl_delay();
    uart->lcrm = 0x0;
    uart_ctrl_delay();
    switch (cfg->baud) { /* FIXME? */
    case 115200:
        uart->lcrl = 0x3;
        break;
    case 2400:
        uart->lcrl = 0xbf;
        break;
    default:
        panic("invalid baud rate %d", cfg->baud);
    }

    /* Initial UART setup */
    uart_ctrl_delay();
    uart->ctrl = 0; /* disabled */
    uart_ctrl_delay();
    uart->lcrh =
          ((cfg->bits - 5) << 5)
        | (cfg->fifos       ? FEN_MASK  : 0)
        | (cfg->stop2       ? STP2_MASK : 0)
        | (cfg->parity_even ? EPS_MASK  : 0)
        | (cfg->parity      ? PEN_MASK  : 0)
        | 0 /* BRK */;

    /* Re-enable UART, interrupts */
    uart_ctrl_delay();
    ctrl = UARTEN_MASK;
    if (intrs) {
        ctrl |= RIEN_MASK | TIEN_MASK;
        if (!cfg->nocts)
            ctrl |= MSIEN_MASK;
    }
    uart->ctrl = ctrl;
}

static struct serialcfg default_cfg = {
    .uart        = 0xff,  /* Arbitrary */
    .fifos       = true,
    .nocts       = true,  /* Arbitrary */
    .baud        = 115200,
    .parity      = false,
    .parity_even = false,
    .stop2       = false,
    .bits        = 8
};

static void serialsrv_cleanup1(void) { serialsrv_cleanup(COM1); }
static void serialsrv_cleanup2(void) { serialsrv_cleanup(COM2); }

static void
serialsrv_cleanup(int port)
{
    serialsrv_uart_setup(&default_cfg, false, get_uart(port));
#ifdef SERIAL_STATS
    int32_t mean, var, min, max;
    mean = stats_mean(tx_delay_stats[port]);
    var  = stats_var(tx_delay_stats[port]);
    min  = tx_delay_stats[port]->min;
    max  = tx_delay_stats[port]->max;
    bwprintf(COM2, "serial%d transmit response time:\n", port + 1);
    bwprintf(COM2, "  mean: %d us\n",   mean * 1000 / 983);
    bwprintf(COM2, "  min:  %d us\n",   min  * 1000 / 983);
    bwprintf(COM2, "  max:  %d us\n",   max  * 1000 / 983);
    bwprintf(COM2, "  var:  %d us^2\n", var  * 1000 / 983);
#endif
}

static void
serial_rx(struct serialsrv *srv, tid_t notifier, int rx_data)
{
    int rc;
    rc = Reply(notifier, NULL, 0);
    assertv(rc, rc == 0);
    if (srv->getc_client < 0) {
        rbuf_putc(&srv->getc_buf, (char)rx_data);
    } else {
        rc = Reply(srv->getc_client, &rx_data, sizeof (rx_data));
        assertv(rc, rc == 0);
        srv->getc_client = -1;
    }
}

static void
serial_putc(struct serialsrv *srv, tid_t client, char c)
{
    int rc, rply;

    if (!srv->cts || !srv->txr) {
        rbuf_putc(&srv->putc_buf, c);
    } else {
        char dummy;
        /* If there are characters waiting, then we should never be
         * ready, since on becoming ready with a character waiting,
         * we immediately send it and stop being ready. */
        assert(!rbuf_peekc(&srv->putc_buf, &dummy));

        /* Ready to send this character! :D */
        serial_writechar(srv, c);
    }

    rply = SERIAL_OK;
    rc = Reply(client, &rply, sizeof (rply));
    assertv(rc, rc == 0);
}

static void
serial_write(struct serialsrv *srv, tid_t client, uint8_t *buf, int len)
{
    int rply, rc;

    rbuf_write(&srv->putc_buf, buf, len);
    if (len > 0 && srv->cts && srv->txr)
        serial_all_ready(srv);

    rply = SERIAL_OK;
    rc = Reply(client, &rply, sizeof (rply));
    assertv(rc, rc == 0);
}

static void
serial_getc(struct serialsrv *srv, tid_t client)
{
    int rply, rc;
    char c;

    /* It's an error to call Getc() on a channel while another
     * task is already waiting for Getc() to return. */
    if (srv->getc_client >= 0) {
        rply = SERIAL_BUSY;
        rc = Reply(client, &rply, sizeof (rply));
        assertv(rc, rc == 0);
        return;
    }

    /* This task gets a character. */
    if (!rbuf_getc(&srv->getc_buf, &c)) {
        srv->getc_client = client; /* It will have to wait */
    } else {
        rply = (int)((uint8_t)c);
        rc = Reply(client, &rply, sizeof (rply));
        assertv(rc, rc == 0);
    }
}

static void
serial_flush(struct serialsrv *srv, tid_t client)
{
    int rc, rply;
    char c;
    if (srv->flush_client >= 0) {
        rply = SERIAL_BUSY;
        rc = Reply(client, &rply, sizeof (rply));
        assertv(rc, rc == 0);
    } else if (!rbuf_peekc(&srv->putc_buf, &c)) {
        rply = SERIAL_OK;
        rc = Reply(client, &rply, sizeof (rply));
        assertv(rc, rc == 0);
    } else {
        srv->flush_client = client;
    }
}

static void
serial_writechar(struct serialsrv *srv, char c)
{
    srv->uart->data = (uint8_t)c;
    srv->uart->ctrl |= TIEN_MASK;
    srv->txr = false;
    srv->cts = srv->nocts;
#ifdef SERIAL_STATS
    if (srv->txr_intr_time >= 0)
        stats_accum(&srv->tx_delay_stats, tmr40_get() - srv->txr_intr_time);
#endif
}

static void
serial_all_ready(struct serialsrv *srv)
{
    int rc, rply;
    char c;
    if (!rbuf_getc(&srv->putc_buf, &c))
        return;
    serial_writechar(srv, c);
    if (srv->flush_client >= 0 && !rbuf_peekc(&srv->putc_buf, &c)) {
        rply = SERIAL_OK;
        rc   = Reply(srv->flush_client, &rply, sizeof (rply));
        assertv(rc, rc == 0);
        srv->flush_client = -1;
    }
}

static void
serial_txr(struct serialsrv *srv, tid_t notifier)
{
    int rc;
    rc = Reply(notifier, NULL, 0);
    assertv(rc, rc == 0);
    srv->txr = true;
    if (srv->cts)
        serial_all_ready(srv);
}

static void
serial_cts(struct serialsrv *srv, tid_t notifier, bool cts)
{
    int rc;
    rc = Reply(notifier, NULL, 0);
    assertv(rc, rc == 0);
    assert(!srv->nocts);
    srv->cts = cts;
    if (srv->cts && srv->txr)
        serial_all_ready(srv);
}

static tid_t
serial_notif_init(
    int evts[2],
    int intrs[2],
    int (*cb)(void*,size_t),
    struct serialcfg *cfg)
{
    tid_t server;
    int rc;

    /* Receive configuration */
    rc = Receive(&server, cfg, sizeof (*cfg));
    assertv(rc, rc == sizeof (*cfg));
    rc = Reply(server, NULL, 0);
    assertv(rc, rc == 0);

    /* Register for UART interrupt */
    assert(cfg->uart < 2);
    rc = RegisterEvent(evts[cfg->uart], intrs[cfg->uart], cb);
    assertv(rc, rc == 0);

    return server;
}

/* Rx Ready Notifier and Callback */
static void
serialrx_notif(void)
{
    static int rx_evts[2]  = { EV_UART1_RX, EV_UART2_RX };
    static int rx_intrs[2] = { IRQ_UART1_RX, IRQ_UART2_RX };
    volatile struct uart *uart;
    struct serialcfg cfg;
    tid_t server;
    struct serialmsg msg;
    int rc;

    server = serial_notif_init(rx_evts, rx_intrs, &serialrx_notif_cb, &cfg);
    uart = get_uart(cfg.uart);

    /* Start actual loop */
    msg.type = SERIALMSG_RX;
    for (;;) {
        rc = AwaitEvent((void*)uart, 0);
        assert(rc >= 0x0 && rc <= 0xff);
        msg.rx_data = rc;
        rc = Send(server, &msg, sizeof (msg), NULL, 0);
        assertv(rc, rc == 0);
    }
}

static int
serialrx_notif_cb(void *untyped_uart, size_t n)
{
    volatile struct uart *uart;
    (void)n; /* ignore */

    /* FIXME: fill buffer while !RXFE */
    uart = (volatile struct uart*)untyped_uart;
    return uart->data;
}

/* Tx Ready Notifier and Callback */
struct serialtx_info {
    volatile struct uart *uart;
    int32_t               txr_time;
};

static void
serialtx_notif(void)
{
    static int tx_evts[2]  = { EV_UART1_TX, EV_UART2_TX };
    static int tx_intrs[2] = { IRQ_UART1_TX, IRQ_UART2_TX };
    struct serialcfg cfg;
    struct serialmsg msg;
    struct serialtx_info info;
    tid_t server;
    int rc;

    server = serial_notif_init(tx_evts, tx_intrs, &serialtx_notif_cb, &cfg);
    info.uart = get_uart(cfg.uart);

    /* Start actual loop */
    msg.type = SERIALMSG_TXR;
    for (;;) {
        rc = AwaitEvent(&info, 0);
        assert(rc == 0);
#ifdef SERIAL_STATS
        msg.txr_time = info.txr_time;
#endif
        rc = Send(server, &msg, sizeof (msg), NULL, 0);
        assertv(rc, rc == 0);
    }
}

static int
serialtx_notif_cb(void *untyped_info, size_t n)
{
    struct serialtx_info *info = untyped_info;
    (void)n; /* ignore */
#ifdef SERIAL_STATS
    info->txr_time = tmr40_get();
#endif
    info->uart->ctrl &= ~TIEN_MASK;
    return 0;
}

/* General UART IRQ Notifier */
struct serialgen_info {
    volatile struct uart *uart;
    struct serialmsg      msg;
};

static void
serialgen_notif(void)
{
    static int gen_evts[2]  = { EV_UART1_GEN, EV_UART2_GEN };
    static int gen_intrs[2] = { IRQ_UART1_GEN, IRQ_UART2_GEN };
    volatile struct uart *uart;
    struct serialcfg cfg;
    tid_t server;
    struct serialgen_info info;
    int rc;

    server = serial_notif_init(gen_evts, gen_intrs, &serialgen_notif_cb, &cfg);
    uart = get_uart(cfg.uart);

    /* Start actual loop */
    info.uart = uart;
    for (;;) {
        rc = AwaitEvent(&info, 0);
        assertv(rc, rc == 0);
        rc = Send(server, &info.msg, sizeof (info.msg), NULL, 0);
        assertv(rc, rc == 0);
    }
}

static int
serialgen_notif_cb(void *untyped_info, size_t n)
{
    struct serialgen_info *info = (struct serialgen_info*)untyped_info;
    uint32_t intr;
    (void)n; /* ignore */

    /* Check interrupt - just modem status for now. */
    intr = info->uart->intr;
    if(!(intr & 0x1)) {
        bwprintf(COM2, "!%x\n", (unsigned)intr);
        return -1; /* not for us */
    }
    info->uart->intr = 0; /* clear modem status interrupt */

    info->msg.type       = SERIALMSG_CTS;
    info->msg.cts_status = (bool)(info->uart->flag & CTS_MASK);
    return 0;
}

static volatile struct uart*
get_uart(int which)
{
    switch (which) {
    case COM1:
        return (volatile struct uart*)UART1_BASE;
    case COM2:
        return (volatile struct uart*)UART2_BASE;
    default:
        panic("invalid UART %d\n", which);
    }
}

void
serialctx_init(struct serialctx *ctx, int channel)
{
    assert(channel == COM1 || channel == COM2);
    char name[8] = "serial\0";
    name[sizeof (name) - 2] = channel == COM1 ? '1' : '2';
    ctx->serial_tid = WhoIs(name);
}

int
Getc(struct serialctx *ctx)
{
    struct serialmsg msg;
    int rplylen;
    int ch;
    msg.type = SERIALMSG_GETC;
    rplylen = Send(ctx->serial_tid, &msg, sizeof (msg), &ch, sizeof (ch));
    assertv(rplylen, rplylen == sizeof (ch));
    return ch;
}

int
Putc(struct serialctx *ctx, char c)
{
    struct serialmsg msg;
    int rplylen;
    int rply;
    msg.type = SERIALMSG_PUTC;
    msg.putc_arg = c;
    rplylen = Send(ctx->serial_tid, &msg, sizeof (msg), &rply, sizeof (rply));
    assertv(rplylen, rplylen == sizeof (rply));
    return rply;
}

int
Write(struct serialctx *ctx, const void *buf, size_t n)
{
    struct serialmsg msg;
    int rplylen;
    int rply;
    msg.type = SERIALMSG_WRITE;
    while (n > 0) {
        size_t k = n <= WRITE_MAXLEN ? n : WRITE_MAXLEN;
        msg.write.len = k;
        memcpy(msg.write.buf, buf, k);
        rplylen = Send(ctx->serial_tid,
            &msg, sizeof (msg),
            &rply, sizeof (rply));
        assertv(rplylen, rplylen == sizeof (rply));
        n   -= k;
        buf += k;
        if (rply != 0)
            return rply;
    }
    return 0;
}

int
Print(struct serialctx *ctx, const char *s)
{
    return Write(ctx, s, strlen(s));
}

int
Printf(struct serialctx *ctx, const char *fmt, ...)
{
    struct ringbuf r;
    va_list args;
    char mem[512];

    rbuf_init(&r, mem, sizeof (mem));
    va_start(args, fmt);
    rbuf_vprintf(&r, fmt, args); /* FIXME */
    va_end(args);

    return Write(ctx, mem, r.len);
}

int
Flush(struct serialctx *ctx)
{
    struct serialmsg msg;
    int rply, rplylen;
    msg.type = SERIALMSG_FLUSH;
    rplylen = Send(ctx->serial_tid, &msg, sizeof (msg), &rply, sizeof (rply));
    assertv(rplylen, rplylen == sizeof (rply));
    return rply;
}

static void
uart_ctrl_delay(void)
{
    __asm__ (
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
    );
}
