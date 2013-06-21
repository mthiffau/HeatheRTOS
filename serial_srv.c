#include "xbool.h"
#include "xint.h"
#include "u_tid.h"
#include "serial_srv.h"

#include "config.h"
#include "xdef.h"
#include "xarg.h"
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
};

struct serialmsg {
    int type;
    union {
        uint8_t rx_data;
        bool    cts_status;
        uint8_t putc_arg;
    };
};

struct serialsrv {
    /* Serial server parameters */
    volatile struct uart *uart;
    bool                  fifos;
    bool                  nocts;

    /* Transmit state */
    bool txr;
    bool cts;

    /* Getc state */
    tid_t          getc_client; /* negative if none */
    struct ringbuf getc_buf;

    /* Buffer memory */
    char getc_buf_mem[GETC_BUF_SIZE];
};

static void serialsrv_init(struct serialsrv *srv, struct serialcfg *cfg);

static void serial_rx(struct serialsrv *srv, tid_t client, int rx_data);
static void serial_getc(struct serialsrv *srv, tid_t client);

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
            /* Tx ready */
            panic("TXR message not implemented");
            break;
        case SERIALMSG_CTS:
            /* CTS state changed */
            panic("CTS message not implemented");
            break;
        case SERIALMSG_GETC:
            /* Char input request from a task */
            serial_getc(&srv, client);
            break;
        case SERIALMSG_PUTC:
            /* Char output request from a task */
            panic("Putc() message not implemented");
            break;
        default:
            panic("unrecognized serial server message %d", msg.type);
        }
    }
}

static void
serialsrv_init(struct serialsrv *srv, struct serialcfg *cfg)
{
    int rc;
    tid_t tid;

    srv->uart  = get_uart(cfg->uart);
    srv->fifos = cfg->fifos;
    srv->nocts = cfg->nocts;

    srv->txr = false; /* interrupt will notify us, even if immediately */
    srv->cts = srv->nocts || (bool)(srv->uart->flag & CTS_MASK);

    srv->getc_client = -1;
    rbuf_init(&srv->getc_buf, srv->getc_buf_mem, sizeof (srv->getc_buf_mem));

    /* Start notifiers */
    tid = Create(PRIORITY_MAX, &serialrx_notif);
    assertv(tid, tid >= 0);
    rc = Send(tid, cfg, sizeof (*cfg), NULL, 0);
    assertv(rc, rc == 0);

    (void)&serialtx_notif;
    (void)&serialgen_notif;
    /* TODO
    tid = Create(PRIORITY_MAX, &serialtx_notif);
    assertv(tid, tid >= 0);
    rc = Send(tid, cfg, sizeof (*cfg), NULL, 0);
    assertv(rc, rc == 0);

    tid = Create(PRIORITY_MAX, &serialgen_notif);
    assertv(tid, tid >= 0);
    rc = Send(tid, cfg, sizeof (*cfg), NULL, 0);
    assertv(rc, rc == 0);
    */
}

static void
serial_rx(struct serialsrv *srv, tid_t client, int rx_data)
{
    int rc;
    rc = Reply(client, NULL, 0);
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

static tid_t
serial_notif_init(
    int evt_type,
    int intrs[2],
    int (*cb)(void*,size_t),
    struct serialcfg *cfg)
{
    tid_t server;
    int evt, rc;

    /* Receive configuration */
    rc = Receive(&server, cfg, sizeof (*cfg));
    assertv(rc, rc == sizeof (*cfg));
    rc = Reply(server, NULL, 0);
    assertv(rc, rc == 0);

    /* Register for UART interrupt */
    assert(cfg->uart < 2);
    evt = NOTIFY_PRIO(cfg->uart, evt_type); /* FIXME? sketchy */
    rc = RegisterEvent(evt, intrs[cfg->uart], cb);
    assertv(rc, rc == 0);

    return server;
}

/* Rx Ready Notifier and Callback */
static void
serialrx_notif(void)
{
    static int rx_intrs[2] = { 23, 25 };

    volatile struct uart *uart;
    struct serialcfg cfg;
    tid_t server;

    struct serialmsg msg;
    int rc;

    server = serial_notif_init(
        NOTIFY_PRIO_RX, rx_intrs, &serialrx_notif_cb, &cfg);
    uart = get_uart(cfg.uart);
    uart->ctrl |= RIEN_MASK;

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
static void
serialtx_notif(void)
{
    /* TODO */
    (void)&serialtx_notif_cb;
}

static int
serialtx_notif_cb(void *untyped_uart, size_t n)
{
    (void)untyped_uart;
    (void)n;
    return -1;
}

/* General UART IRQ Notifier */
static void
serialgen_notif(void)
{
    /* TODO */
    (void)&serialgen_notif_cb;
}

static int
serialgen_notif_cb(void *untyped_uart, size_t n)
{
    (void)untyped_uart;
    (void)n;
    return -1;
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
