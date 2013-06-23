#include "xbool.h"
#include "xint.h"
#include "u_tid.h"
#include "tcmux_srv.h"

#include "xdef.h"
#include "u_syscall.h"
#include "ns.h"
#include "clock_srv.h"
#include "serial_srv.h"

#include "queue.h"

#include "xassert.h"
#include "xarg.h"
#include "bwio.h"

#define SPEED_QUEUE_SIZE        64
#define SWITCH_QUEUE_SIZE       64

#define TRCMD_SPEED(n)         ((char)n)
#define TRCMD_REVERSE          ((char)15)
#define TRCMD_SWITCH_STRAIGHT  ((char)33)
#define TRCMD_SWITCH_CURVE     ((char)34)
#define TRCMD_SWITCH_OFF       ((char)32)
#define TRCMD_SENSOR_POLL_ALL  ((char)133)

enum {
    TCMUX_TRAIN_SPEED,
    TCMUX_SWITCH_CURVE,
    TCMUX_CMD_READY,
    TCMUX_SWITCH_READY,
    /* TODO: more satellite messages */
};

struct speed_msg {
    uint8_t train;
    uint8_t speed;
    bool    sync;
};

struct switch_msg {
    uint8_t sw;
    bool    curved;
    bool    sync;
};

struct train_cmd {
    char    cmd[2];
    uint8_t len;
    bool    is_switch;
    tid_t   client;
};

struct tcmuxmsg {
    int type;
    union {
        struct speed_msg  speed;
        struct switch_msg sw;
    };
};

struct tcmux {
    struct serialctx  port;
    bool              cmd_ready;
    bool              switch_ready;
    tid_t             flusher;
    tid_t             switch_delayer;
    tid_t             last_cmd_client;
    bool              last_cmd_was_switch;
    tid_t             last_switchcmd_client;

    /* Command queues */
    struct queue      speed_cmdq, switch_cmdq;
    struct train_cmd  speed_cmdq_mem[SPEED_QUEUE_SIZE];
    struct train_cmd  switch_cmdq_mem[SWITCH_QUEUE_SIZE];
    bool              solenoid_on;
};

static void tcmux_init(struct tcmux*);
static void tcmuxsrv_train_speed(
    struct tcmux *tc, tid_t client, struct speed_msg *m);
static void tcmuxsrv_switch_curve(
    struct tcmux *tc, tid_t client, struct switch_msg *m);
static void tcmuxsrv_cmd_ready(struct tcmux *tc, tid_t client);
static void tcmuxsrv_switch_ready(struct tcmux *tc, tid_t client);

static void tcmuxsrv_trcmd_trysend(struct tcmux *tc);
static void tcmuxsrv_trcmd_send(struct tcmux *tc, struct train_cmd *trcmd);

static void tcmux_cmd_flusher(void);
static void tcmux_switch_delayer(void);

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
        case TCMUX_CMD_READY:
            tcmuxsrv_cmd_ready(&tc, client);
            break;
        case TCMUX_SWITCH_READY:
            tcmuxsrv_switch_ready(&tc, client);
            break;
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
    tc->cmd_ready             = false;
    tc->switch_ready          = false;
    tc->last_cmd_client       = -1;
    tc->last_cmd_was_switch   = false;
    tc->last_switchcmd_client = -1;

    /* Initialize command queues */
    q_init(
        &tc->speed_cmdq,
        tc->speed_cmdq_mem,
        sizeof (tc->speed_cmdq_mem[0]),
        sizeof (tc->speed_cmdq_mem));
    q_init(
        &tc->switch_cmdq,
        tc->switch_cmdq_mem,
        sizeof (tc->switch_cmdq_mem[0]),
        sizeof (tc->switch_cmdq_mem));

    tc->solenoid_on = false;

    /* Start train command flusher */
    tc->flusher = Create(1, tcmux_cmd_flusher);
    assert(tc->flusher >= 0);

    /* Start switch command delayer */
    tc->switch_delayer = Create(1, &tcmux_switch_delayer);
    assert(tc->switch_delayer >= 0);
}

static void
tcmuxsrv_cmd_ready(struct tcmux *tc, tid_t client)
{
    int rc;
    assert(client == tc->flusher);
    tc->cmd_ready = true;
    if (tc->last_cmd_was_switch) {
        tc->switch_ready = false;
        tc->last_switchcmd_client = tc->last_cmd_client;
        rc = Reply(tc->switch_delayer, NULL, 0);
        assertv(rc, rc == 0);
    } else if (tc->last_cmd_client >= 0) {
        rc = Reply(tc->last_cmd_client, NULL, 0);
        assertv(rc, rc == 0);
    }
    tcmuxsrv_trcmd_trysend(tc);
}

static void
tcmuxsrv_switch_ready(struct tcmux *tc, tid_t client)
{
    int rc;
    assert(client == tc->switch_delayer);
    tc->switch_ready = true;
    if (tc->last_switchcmd_client >= 0) {
        rc = Reply(tc->last_switchcmd_client, NULL, 0);
        assertv(rc, rc == 0);
    }
    tcmuxsrv_trcmd_trysend(tc);
}

static void
tcmuxsrv_train_speed(
    struct tcmux *tc,
    tid_t client,
    struct speed_msg *m)
{
    int rc;
    struct train_cmd trcmd;

    trcmd.cmd[0]    = TRCMD_SPEED(m->speed);
    trcmd.cmd[1]    = (char)m->train;
    trcmd.len       = 2;
    trcmd.is_switch = false;
    if (m->sync) {
        trcmd.client = client;
    } else {
        rc = Reply(client, NULL, 0);
        assertv(rc, rc == 0);
        trcmd.client = -1;
    }

    rc = q_enqueue(&tc->speed_cmdq, &trcmd);
    assertv(rc, rc == 0);
    tcmuxsrv_trcmd_trysend(tc);
}

static void
tcmuxsrv_switch_curve(
    struct tcmux *tc,
    tid_t client,
    struct switch_msg *m)
{
    int rc;
    struct train_cmd trcmd;

    trcmd.cmd[0]    = m->curved ? TRCMD_SWITCH_CURVE : TRCMD_SWITCH_STRAIGHT;
    trcmd.cmd[1]    = (char)m->sw;
    trcmd.len       = 2;
    trcmd.is_switch = true;
    if (m->sync) {
        trcmd.client = client;
    } else {
        rc = Reply(client, NULL, 0);
        assertv(rc, rc == 0);
        trcmd.client = -1;
    }

    rc = q_enqueue(&tc->switch_cmdq, &trcmd);
    assertv(rc, rc == 0);

    tcmuxsrv_trcmd_trysend(tc);
}

static void
tcmuxsrv_trcmd_trysend(struct tcmux *tc)
{
    struct train_cmd trcmd;
    if (!tc->cmd_ready)
        return;

    if (tc->switch_ready) {
        if (q_dequeue(&tc->switch_cmdq, &trcmd)) {
            tc->solenoid_on = true;
            tcmuxsrv_trcmd_send(tc, &trcmd);
            return;
        } else if (tc->solenoid_on) {
            trcmd.cmd[0] = TRCMD_SWITCH_OFF;
            trcmd.len       = 1;
            trcmd.client    = -1;
            trcmd.is_switch = false;
            tcmuxsrv_trcmd_send(tc, &trcmd);
            tc->solenoid_on = false;
            return;
        }
    }

    if (q_dequeue(&tc->speed_cmdq, &trcmd))
        tcmuxsrv_trcmd_send(tc, &trcmd);
}

static void
tcmuxsrv_trcmd_send(struct tcmux *tc, struct train_cmd *trcmd)
{
    int rc;
    assert(tc->cmd_ready);
    rc = Write(&tc->port, trcmd->cmd, trcmd->len);
    assertv(rc, rc == 0);
    rc = Reply(tc->flusher, NULL, 0);
    assertv(rc, rc == 0);
    tc->cmd_ready = false;
    tc->last_cmd_client = trcmd->client;
    tc->last_cmd_was_switch = trcmd->is_switch;
}

static void
tcmux_cmd_flusher(void)
{
    struct serialctx port;
    struct tcmuxmsg msg;
    tid_t tcmux;
    int rc, rplylen;

    tcmux = MyParentTid();
    serialctx_init(&port, COM1);

    msg.type = TCMUX_CMD_READY;
    for (;;) {
        rplylen = Send(tcmux, &msg, sizeof (msg), NULL, 0);
        assertv(rplylen, rplylen == 0);
        rc = Flush(&port);
        assertv(rc, rc == SERIAL_OK);
    }
}

static void
tcmux_switch_delayer(void)
{
    struct clkctx clock;
    struct tcmuxmsg msg;
    tid_t tcmux;
    int rc, rplylen;

    tcmux = MyParentTid();
    clkctx_init(&clock);

    msg.type = TCMUX_SWITCH_READY;
    for (;;) {
        rplylen = Send(tcmux, &msg, sizeof (msg), NULL, 0);
        assertv(rplylen, rplylen == 0);
        rc = Delay(&clock, 10);
        assertv(rc, rc == CLOCK_OK);
    }
}

void
tcmuxctx_init(struct tcmuxctx *tcmux)
{
    tcmux->tcmuxsrv_tid = WhoIs("tcmux");
}

static void
tcmux_train_speed_impl(
    struct tcmuxctx *tc,
    uint8_t train,
    uint8_t speed,
    bool sync)
{
    int rplylen;
    struct tcmuxmsg msg = {
        .type  = TCMUX_TRAIN_SPEED,
        .speed = {
            .train = train,
            .speed = speed,
            .sync  = sync
        }
    };
    rplylen = Send(tc->tcmuxsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

static void
tcmux_switch_curve_impl(
    struct tcmuxctx *tc,
    uint8_t sw,
    bool curved,
    bool sync)
{
    int rplylen;
    struct tcmuxmsg msg = {
        .type  = TCMUX_SWITCH_CURVE,
        .sw = {
            .sw     = sw,
            .curved = curved,
            .sync   = sync
        }
    };
    rplylen = Send(tc->tcmuxsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

void
tcmux_train_speed(struct tcmuxctx *tc, uint8_t train, uint8_t speed)
{
    tcmux_train_speed_impl(tc, train, speed, false);
}

void
tcmux_train_speed_sync(struct tcmuxctx *tc, uint8_t train, uint8_t speed)
{
    tcmux_train_speed_impl(tc, train, speed, true);
}

void
tcmux_switch_curve(struct tcmuxctx *tc, uint8_t sw, bool curved)
{
    tcmux_switch_curve_impl(tc, sw, curved, false);
}

void
tcmux_switch_curve_sync(struct tcmuxctx *tc, uint8_t sw, bool curved)
{
    tcmux_switch_curve_impl(tc, sw, curved, true);
}
