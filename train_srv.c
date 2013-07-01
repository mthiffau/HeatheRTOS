#include "xbool.h"
#include "xint.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "u_tid.h"
#include "train_srv.h"

#include "xdef.h"
#include "xarg.h"
#include "xassert.h"
#include "ringbuf.h"

#include "u_syscall.h"
#include "ns.h"
#include "tcmux_srv.h"

#define TRAIN_DEFSPEED      8

enum {
    TRAINMSG_SETSPEED,
    TRAINMSG_MOVETO,
    TRAINMSG_STOP,
    TRAINMSG_SENSOR,
};

struct trainmsg {
    int type;
    union {
        uint8_t speed;
        int     dest_node_id;
    };
};

struct train {
    const struct track_graph *track;
    struct tcmuxctx           tcmux;
    uint8_t                   train_id;
    const struct track_node  *landmark;
    const struct track_node  *dest;
    uint8_t                   speed;
};

static void trainsrv_init(struct train *tr, struct traincfg *cfg);
static void trainsrv_setspeed(struct train *tr, uint8_t speed);
static void trainsrv_moveto(struct train *tr, const struct track_node *dest);
static void trainsrv_stop(struct train *tr);
static void trainsrv_sensor(struct train *tr);

static void trainsrv_sensor_listen(void);
static void trainsrv_empty_reply(tid_t client);
static void trainsrv_fmt_name(uint8_t train_id, char buf[32]);

void
trainsrv_main(void)
{
    char srv_name[32];
    struct train tr;
    struct traincfg cfg;
    struct trainmsg msg;
    int rc, msglen;
    tid_t client;

    msglen = Receive(&client, &cfg, sizeof (cfg));
    assertv(msglen, msglen == sizeof (cfg));
    rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);

    trainsrv_init(&tr, &cfg);
    trainsrv_fmt_name(tr.train_id, srv_name);
    rc = RegisterAs(srv_name);
    assertv(rc, rc == 0);

    for (;;) {
        msglen = Receive(&client, &msg, sizeof (msg));
        assertv(msglen, msglen == sizeof (msg));
        switch (msg.type) {
        case TRAINMSG_SETSPEED:
            trainsrv_empty_reply(client);
            trainsrv_setspeed(&tr, msg.speed);
            break;
        case TRAINMSG_MOVETO:
            trainsrv_empty_reply(client);
            trainsrv_moveto(&tr, &tr.track->nodes[msg.dest_node_id]);
            break;
        case TRAINMSG_STOP:
            trainsrv_empty_reply(client);
            trainsrv_stop(&tr);
            break;
        case TRAINMSG_SENSOR:
            trainsrv_sensor(&tr);
            break;
        default:
            panic("invalid train message type %d", msg.type);
        }
    }
}

static void
trainsrv_init(struct train *tr, struct traincfg *cfg)
{
    tr->track    = all_tracks[cfg->track_id];
    tr->train_id = cfg->train_id;
    tr->speed    = TRAIN_DEFSPEED;
    tr->landmark = &tr->track->nodes[24]; /* B9 - FIXME */
    tr->dest     = NULL;
    tcmuxctx_init(&tr->tcmux);

    (void)trainsrv_sensor_listen; /* TODO start sensor listener task */
}

static void
trainsrv_setspeed(struct train *tr, uint8_t speed)
{
    assert(speed > 0 && speed < 15);
    tr->speed = speed;
    if (tr->dest != NULL)
        tcmux_train_speed(&tr->tcmux, tr->train_id, speed);
}

static void
trainsrv_moveto(struct train *tr, const struct track_node *dest)
{
    const struct track_node *path[TRACK_NODES_MAX];
    int i, path_len;
    path_len = track_pathfind(tr->track, tr->landmark, dest, path);
    if (path_len < 0)
        return;
    tr->dest = dest;
    for (i = 0; i < path_len - 1; i++) {
        bool curved;
        if (path[i]->type != TRACK_NODE_BRANCH)
            continue;
        curved = path[i]->edge[TRACK_EDGE_CURVED].dest == path[i + 1];
        tcmux_switch_curve(&tr->tcmux, path[i]->num, curved);
    }
    tcmux_train_speed(&tr->tcmux, tr->train_id, tr->speed);
}

static void
trainsrv_stop(struct train *tr)
{
    tr->dest = NULL;
    tcmux_train_speed(&tr->tcmux, tr->train_id, 0);
}

static void
trainsrv_sensor(struct train *tr)
{
    /* TODO */
    (void)tr;
}

static void
trainsrv_sensor_listen(void)
{
    /* TODO */
}

static void
trainsrv_empty_reply(tid_t client)
{
    int rc = Reply(client, NULL, 0);
    assertv(rc, rc == 0);
}

void
trainctx_init(
    struct trainctx *ctx,
    const struct track_graph *track,
    uint8_t train_id)
{
    char buf[32];
    ctx->track = track;
    trainsrv_fmt_name(train_id, buf);
    ctx->trainsrv_tid = WhoIs(buf);
}

void
train_setspeed(struct trainctx *ctx, uint8_t speed)
{
    struct trainmsg msg;
    int rplylen;
    msg.type  = TRAINMSG_SETSPEED;
    msg.speed = speed;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

void
train_moveto(struct trainctx *ctx, const struct track_node *dest)
{
    struct trainmsg msg;
    int rplylen;
    msg.type         = TRAINMSG_MOVETO;
    msg.dest_node_id = dest - ctx->track->nodes;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

void
train_stop(struct trainctx *ctx)
{
    struct trainmsg msg;
    int rplylen;
    msg.type = TRAINMSG_STOP;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

static void
trainsrv_fmt_name(uint8_t train_id, char buf[32])
{
    struct ringbuf rbuf;
    rbuf_init(&rbuf, buf, 32);
    rbuf_printf(&rbuf, "train%d", train_id);
    rbuf_putc(&rbuf, '\0');
}
