#include "u_tid.h"

#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "xarg.h"
#include "bwio.h"
#include "timer.h"

#include "xassert.h"
#include "u_syscall.h"
#include "u_events.h"
#include "ns.h"
#include "array_size.h"

enum {
    SERIALMSG_RXR,   /* Rx Ready */
    SERIALMSG_RXTO,  /* FIFO timeout */
    SERIALMSG_TXR,   /* Tx Ready */
    SERIALMSG_CTS,   /* CTS status */
    SERIALMSG_GETC,  /* Request for a character */
    SERIALMSG_PUTC   /* Request to put a character */
};

enum {
    SERIALRPLY_OK,   /* All good */
    SERIALRPLY_TXBF, /* Server tx buffer full */
};

enum {
    NOTIF_CFG /* Tell the notifier what uart and fifo state */
};

struct serialmsg {
    uint8_t type;
    uint8_t len;
    int8_t data[17];
};

struct serialsrv {
    uint8_t uart;
    bool fifo_on;
    bool txr;
    bool rxr;
    bool ctsr;

    tid_t rx_notif;
    tid_t tx_notif;
    tid_t gen_notif;
};

static void serialsrv_init(void);

static void serialrx_notif(void);
static void serialrx_notif_cb(void);

static void serialtx_notif(void);
static void serialtx_notif_cb(void);

/* This notifier is for the rest of the
 UART interrupts which aren't indivually
 registerable (like modem status) */
static void serialgen_notif(void);
static void serialgen_notif_cb(void);

void
serialsrv_main(void)
{
    int rc;

    struct serialsrv srv;
    struct serialmsg msg;

    serialsrv_init(&srv);

    rc = RegisterAs("serial1");
    assertv(rc, rc == 0);

    for (;;) {
        rc = Receive(&client, &msg, sizeof(msg));
        assert(rc == sizeof(msg));

        switch(msg.type) {
        case SERIALMSG_RXR:
            /* Rx ready */
            break;
        case SERIALMSG_RXTO:
            /* Rx timeout */
            break;
        case SERIALMSG_TXR:
            /* Tx ready */
            break;
        case SERIALMSG_CTS:
            /* CTS state changed */
            break;
        case SERIALMSG_GETC:
            /* Char input request from a task */
            break;
        case SERIALMSG_PUTC:
            /* Char output request from a task */
            break;
        default:
            panic("unrecognized serial server message %d", msg.type);
        }
    }
}

static void
serialsrv_init(serialsrv* srv, uint8_t uart, bool fifo_on)
{
    int rc;
    struct serialmsg msg;

    srv->uart = uart;
    srv->fifo_on = fifo_on;

    msg.data[0] = uart;
    msg.data[1] = fifon_on;

    srv->rx_notif = Create(PRIORITY_MAX, &serialtx_notif);
    assert(srv->rx_notif == 0);
    rc = Send(srv->rx_notif, &msg, sizeof(msg), NULL, 0);
    assertv(rc, rc == 0);

    srv->tx_notif = Create(PRIORITY_MAX, &serialrx_notif);
    assert(srv->tx_notif == 0);
    rc = Send(srv->tx_notif, &msg, sizeof(msg), NULL, 0);
    assertv(rc, rc == 0);

    srv->gen_notif = Create(PRIORITY_MAX, &serialcts_notif);
    assert(srv->gen_notif == 0);
    rc = Send(srv->gen_notif, &msg, sizeof(msg), NULL, 0);
    assertv(rc, rc == 0);
}

/* Rx Ready Notifier and Callback */
static void
serialrx_notif(void)
{
    int uart, rc;
    bool fifo_on;

    tid_t server;

    struct serialmsg msg;

    /* Receive configuration */
    rc = Receive(&server, &msg, sizeof(msg));
    assertv(rc, rc == sizeof(msg));
    assert(msg.type == NOTIF_CFG);

    uart    = msg.data[0];
    fifo_on = msg.data[1];

    /* Init the hardware */

    rc = Reply(server, NULL, 0);

    /* Start actual loop */
}

static void
serialrx_notif_cb(void)
{


}

/* Tx Ready Notifier and Callback */
static void
serialtx_notif(void)
{
    int uart, rc;
    bool fifo_on;

    tid_t server;

    struct serialmsg msg;

    /* Receive configuration */
    rc = Receive(&server, &msg, sizeof(msg));
    assertv(rc, rc == sizeof(msg));
    assert(msg.type == NOTIF_CFG);

    uart    = msg.data[0];
    fifo_on = msg.data[1];

    /* Init the hardware */

    rc = Reply(server, NULL, 0);

    /* Start actual loop */
}

static void
serialtx_notif_cb(void)
{


}

/* General UART IRQ Notifier */
static void
serialgen_notif(void)
{
    int uart, rc;
    bool fifo_on;

    tid_t server;

    struct serialmsg msg;

    /* Receive configuration */
    rc = Receive(&server, &msg, sizeof(msg));
    assertv(rc, rc == sizeof(msg));
    assert(msg.type == NOTIF_CFG);

    uart    = msg.data[0];
    fifo_on = msg.data[1];

    /* Init the hardware */

    rc = Reply(server, NULL, 0);

    /* Start actual loop */
}

static void
serialgen_notif_cb(void)
{


}

