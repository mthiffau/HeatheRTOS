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

#define WRITE_MAXLEN    32
#define GETC_BUF_SIZE   (1 << 15) /* 32 kB */
#define PUTC_BUF_SIZE   (1 << 15) /* 32 kB */
#define UART_FIFO_SIZE  16

/* It's possible for a character to come in while we're draining the FIFO,
 * but (as far as we know) no more than 1. But paranoia, so 4. */
#define RX_BUF_SIZE    (UART_FIFO_SIZE + 4)

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
        bool    cts_status;
        uint8_t putc_arg;
        struct {
            int     len;
            uint8_t buf[WRITE_MAXLEN];
        } write;
        struct {
            int     len;
            uint8_t buf[RX_BUF_SIZE];
        } rx;
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
};

static void serialsrv_init(struct serialsrv *srv, struct serialcfg *cfg);
static void serialsrv_cleanup1(void);
static void serialsrv_cleanup2(void);
static void serialsrv_cleanup(int port);
static void serialsrv_uart_setup(
    const struct serialcfg *cfg, bool intrs, volatile struct uart *uart);

static void serial_writechars(struct serialsrv *srv);
static void serial_rx(struct serialsrv*, tid_t, uint8_t[RX_BUF_SIZE], int);
static void serial_txr(struct serialsrv *srv, tid_t notifier);
static void serial_cts(struct serialsrv *srv, tid_t notifier, bool cts);
static void serial_putc(struct serialsrv *srv, tid_t client, char c);
static void serial_write(
    struct serialsrv *srv, tid_t client, uint8_t *buf, int len);
static void serial_getc(struct serialsrv *srv, tid_t client);
static void serial_flush(struct serialsrv *srv, tid_t client);

static tid_t serial_notif_init(
    const int irqs[2], int (*cb)(void*,size_t), struct serialcfg *cfg);

/* Notifier for all UART interrupts,
 * including those that aren't separately signalled. */
static void serial_notif(void);
static int  serial_notif_cb(void*, size_t);

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
            serial_rx(&srv, client, msg.rx.buf, msg.rx.len);
            break;
        case SERIALMSG_TXR:
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
    static void (*const cleanups[2])(void) = {
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

    /* Make sure there's no left-over input in the UART holding register */
    while (!(srv->uart->flag & RXFE_MASK)) {
        int dummy = srv->uart->data;
        (void)dummy;
    }

    /* Initialize getc state */
    srv->getc_client = -1;
    rbuf_init(&srv->getc_buf, srv->getc_buf_mem, sizeof (srv->getc_buf_mem));

    /* Initialize putc state */
    uart_ctrl_delay();
    srv->txr = false; /* interrupt will notify us, even if immediately */
    srv->cts = srv->nocts || (bool)(srv->uart->flag & CTS_MASK);
    rbuf_init(&srv->putc_buf, srv->putc_buf_mem, sizeof (srv->putc_buf_mem));
    srv->flush_client = -1;

    /* Start notifiers */
    tid = Create(PRIORITY_MAX, &serial_notif);
    assertv(tid, tid >= 0);
    rc = Send(tid, cfg, sizeof (*cfg), NULL, 0);
    assertv(rc, rc == 0);
}

static void
serialsrv_uart_setup(
    const struct serialcfg* cfg,
    bool intrs,
    volatile struct uart *uart)
{
    uint32_t ctrl;

    /* Initial UART setup */
    uart_ctrl_delay();
    uart->ctrl = 0; /* disabled */

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
        if (cfg->fifos)
            ctrl |= RTIEN_MASK;
        if (!cfg->nocts)
            ctrl |= MSIEN_MASK;
    }
    uart->ctrl = ctrl;
}

static const struct serialcfg default_cfg = {
    .uart        = 0xff,  /* Arbitrary */
    .fifos       = false,
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
}

static void
serial_rx(
    struct serialsrv *srv,
    tid_t notifier,
    uint8_t buf[RX_BUF_SIZE],
    int len)
{
    int rc, rply;
    rc = Reply(notifier, NULL, 0);
    assertv(rc, rc == 0);
    assert(len >= 1);
    if (srv->getc_client < 0) {
        assert(srv->getc_buf.len + len <= srv->getc_buf.size);
        rbuf_write(&srv->getc_buf, buf, len);
    } else {
        rply = buf[0];
        rc = Reply(srv->getc_client, &rply, sizeof (rply));
        assertv(rc, rc == 0);
        srv->getc_client = -1;
        assert(srv->getc_buf.len + len - 1 <= srv->getc_buf.size);
        rbuf_write(&srv->getc_buf, buf + 1, len - 1);
    }
}

static void
serial_putc(struct serialsrv *srv, tid_t client, char c)
{
    int rc, rply;

    rbuf_putc(&srv->putc_buf, c);
    if (srv->cts && srv->txr)
        serial_writechars(srv);

    rply = SERIAL_OK;
    rc = Reply(client, &rply, sizeof (rply));
    assertv(rc, rc == 0);
}

static void
serial_write(struct serialsrv *srv, tid_t client, uint8_t *buf, int len)
{
    int rply, rc;

    assert(srv->putc_buf.len + len <= srv->putc_buf.size);
    rbuf_write(&srv->putc_buf, buf, len);
    if (len > 0 && srv->cts && srv->txr)
        serial_writechars(srv);

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
serial_writechars(struct serialsrv *srv)
{
    int i, lim;
    char c;
    /* With FIFOs off, the TXFF flag doesn't always get set quickly
     * enough here for us to rely on it. So we need to explicitly
     * limit our write to a single character. */
    lim = srv->fifos ? 8 : 1;
    for (i = 0; i < lim; i++) {
        if (!rbuf_getc(&srv->putc_buf, &c))
            break;
        srv->uart->data = (uint8_t)c;
    }

    if (i == 0)
        return;

    srv->uart->ctrl |= TIEN_MASK;
    srv->txr = false;
    srv->cts = srv->nocts;
    if (srv->flush_client >= 0 && !rbuf_peekc(&srv->putc_buf, &c)) {
        int rc, rply;
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
        serial_writechars(srv);
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
        serial_writechars(srv);
}

static tid_t
serial_notif_init(
    const int irqs[2],
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
    rc = RegisterEvent(irqs[cfg->uart], cb);
    assertv(rc, rc == 0);

    return server;
}

/* General UART IRQ Notifier */
struct snotify_info {
    volatile struct uart *uart;
    struct serialmsg      msg;
};

static void
serial_notif(void)
{
    static const int gen_irqs[2] = { IRQ_UART1_GEN, IRQ_UART2_GEN };
    volatile struct uart *uart;
    struct serialcfg cfg;
    tid_t server;
    struct snotify_info info;
    int rc;

    server = serial_notif_init(gen_irqs, &serial_notif_cb, &cfg);
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
serial_notif_cb(void *untyped_info, size_t dummy)
{
    struct snotify_info *info = (struct snotify_info*)untyped_info;
    uint32_t intr;
    (void)dummy; /* ignore */

    /* Check interrupt - just modem status for now. */
    intr = info->uart->intr;
    if (intr & (RIS_MASK | RTIS_MASK)) {
        int n;
        info->msg.type = SERIALMSG_RX;
        n = 0;
        do {
            info->msg.rx.buf[n++] = info->uart->data;
        } while (!(info->uart->flag & RXFE_MASK));
        info->msg.rx.len = n;
        if (n == 0)
            panic("read zero characters from 0x%x", (uint32_t)info->uart);
    } else if (intr & TIS_MASK) {
        info->msg.type = SERIALMSG_TXR;
        info->uart->ctrl &= ~TIEN_MASK;
    } else if (intr & MIS_MASK) {
        info->msg.type = SERIALMSG_CTS;
        info->msg.cts_status = (bool)(info->uart->flag & CTS_MASK);
        info->uart->intr = 0; /* clear modem status interrupt */
    } else {
        panic("fatal: mystery UART interrupt");
    }

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