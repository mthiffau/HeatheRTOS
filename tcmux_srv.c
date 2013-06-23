#include "xbool.h"
#include "xint.h"
#include "u_tid.h"
#include "tcmux_srv.h"

#include "xdef.h"
#include "u_syscall.h"
#include "ns.h"
#include "clock_srv.h"
#include "serial_srv.h"

#include "xassert.h"
#include "xarg.h"
#include "bwio.h"

#define TRCMD_SPEED(n)         ((char)n)
#define TRCMD_REVERSE          ((char)15)
#define TRCMD_SWITCH_STRAIGHT  ((char)33)
#define TRCMD_SWITCH_CURVE     ((char)34)
#define TRCMD_SWITCH_OFF       ((char)32)
#define TRCMD_SENSOR_POLL_ALL  ((char)133)

enum {
    TCMUX_TRAIN_SPEED,
    TCMUX_SWITCH_CURVE,
    /* TODO: satellite messages */
};

struct tcmux {
    struct serialctx port;
};

struct speed_msg {
    uint8_t train;
    uint8_t speed;
};

struct switch_msg {
    uint8_t sw;
    bool curved;
};

struct tcmuxmsg {
    int type;
    union {
        struct speed_msg  speed;
        struct switch_msg sw;
    };
};

static void tcmux_init(struct tcmux*);
static void tcmuxsrv_train_speed(
    struct tcmux *tc, tid_t client, struct speed_msg *m);
static void tcmuxsrv_switch_curve(
    struct tcmux *tc, tid_t client, struct switch_msg *m);

void
tcmuxsrv_main(void)
{
    struct tcmuxmsg msg;
    struct tcmux tc;
    int msglen;
    tid_t client;

    tcmux_init(&tc);
    RegisterAs("tcmux");

    for (;;) {
        msglen = Receive(&client, &msg, sizeof (msg));
        assertv(msglen, msglen == sizeof (msg));
        switch (msg.type) {
        case TCMUX_TRAIN_SPEED:
            tcmuxsrv_train_speed(&tc, client, &msg.speed);
            break;
        case TCMUX_SWITCH_CURVE:
            tcmuxsrv_switch_curve(&tc, client, &msg.sw);
            break;
        default:
            panic("Invalid tcmux message type %d\n", msg.type);
        }
    }
}

static void
tcmux_init(struct tcmux *tc)
{
    serialctx_init(&tc->port, COM1);
}

static void
tcmuxsrv_train_speed(
    struct tcmux *tc,
    tid_t client,
    struct speed_msg *m)
{
    int rc;
    char cmd[2];

    rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);

    cmd[0] = TRCMD_SPEED(m->speed);
    cmd[1] = (char)m->train;
    Write(&tc->port, cmd, sizeof (cmd));
}

static void
tcmuxsrv_switch_curve(
    struct tcmux *tc,
    tid_t client,
    struct switch_msg *m)
{
    int rc;
    char cmd[3];

    rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);

    cmd[0] = m->curved ? TRCMD_SWITCH_CURVE : TRCMD_SWITCH_STRAIGHT;
    cmd[1] = (char)m->sw;
    cmd[2] = TRCMD_SWITCH_OFF;
    Write(&tc->port, cmd, sizeof (cmd));
}

void
tcmuxctx_init(struct tcmuxctx *tcmux)
{
    tcmux->tcmuxsrv_tid = WhoIs("tcmux");
}

void
tcmux_train_speed(struct tcmuxctx *tc, uint8_t train, uint8_t speed)
{
    int rplylen;
    struct tcmuxmsg msg = {
        .type  = TCMUX_TRAIN_SPEED,
        .speed = {
            .train = train,
            .speed = speed
        }
    };
    rplylen = Send(tc->tcmuxsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

void
tcmux_switch_curve(struct tcmuxctx *tc, uint8_t sw, bool curved)
{
    int rplylen;
    struct tcmuxmsg msg = {
        .type  = TCMUX_SWITCH_CURVE,
        .sw = {
            .sw     = sw,
            .curved = curved
        }
    };
    rplylen = Send(tc->tcmuxsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}
