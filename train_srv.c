#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "bithack.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "u_tid.h"
#include "train_srv.h"

#include "xdef.h"
#include "xarg.h"
#include "xassert.h"
#include "xmemcpy.h"
#include "ringbuf.h"

#include "u_syscall.h"
#include "ns.h"
#include "tcmux_srv.h"
#include "sensor_srv.h"

#define TRAIN_DEFSPEED          8
#define TRAIN_CRAWLSPEED        2

/* FIXME these should be looked-up per train */
#define TRAIN_FRONT_OFFS_MM     30
#define TRAIN_BACK_OFFS_MM      145

enum {
    TRAINMSG_SETSPEED,
    TRAINMSG_MOVETO,
    TRAINMSG_STOP,
    TRAINMSG_SENSOR,
};

enum {
    TRAIN_DISORIENTED,
    TRAIN_ORIENTING,
    TRAIN_STOPPED,
    TRAIN_ENROUTE,
};

struct trainmsg {
    int type;
    union {
        uint8_t   speed;
        int       dest_node_id;
        sensors_t sensors[SENSOR_MODULES];
    };
};

struct sensor_reply {
    sensors_t sensors[SENSOR_MODULES];
    int       timeout;
};

struct train {
    int                       state;
    const struct track_graph *track;
    struct tcmuxctx           tcmux;
    uint8_t                   train_id;
    const struct track_node  *ahead, *behind;
    int                       ahead_mm, behind_mm;
    const struct track_node  *path[TRACK_NODES_MAX];
    int                       path_len, path_next, path_sensnext;
    uint8_t                   speed;
    tid_t                     sensor_tid;
    bool                      sensor_rdy;
};

static void trainsrv_init(struct train *tr, struct traincfg *cfg);
static void trainsrv_setspeed(struct train *tr, uint8_t speed);
static void trainsrv_moveto(struct train *tr, const struct track_node *dest);
static void trainsrv_stop(struct train *tr);
static void trainsrv_sensor_disoriented(struct train *tr);
static void trainsrv_sensor_orienting(
    struct train *tr, sensors_t sensors[SENSOR_MODULES]);
static void trainsrv_sensor_enroute(
    struct train *tr, sensors_t sensors[SENSOR_MODULES]);
static void trainsrv_sensor(
    struct train *tr, sensors_t sensors[SENSOR_MODULES]);
static void trainsrv_expect_sensor(struct train *tr, int sensor, int timeout);
static bool trainsrv_expect_next_sensor(struct train *tr);
static void trainsrv_switch_upto_sensnext(struct train *tr);

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
            assert(client == tr.sensor_tid);
            assert(!tr.sensor_rdy);
            trainsrv_sensor(&tr, msg.sensors);
            break;
        default:
            panic("invalid train message type %d", msg.type);
        }
    }
}

static void
trainsrv_init(struct train *tr, struct traincfg *cfg)
{
    tr->state    = TRAIN_DISORIENTED;
    tr->track    = all_tracks[cfg->track_id];
    tr->train_id = cfg->train_id;
    tr->speed    = TRAIN_DEFSPEED;
    tr->ahead    = NULL;
    tr->behind   = NULL;
    tcmuxctx_init(&tr->tcmux);

    tr->sensor_rdy = false;
    tr->sensor_tid = Create(PRIORITY_TRAIN - 1, &trainsrv_sensor_listen);
    assert(tr->sensor_tid >= 0);
}

static void
trainsrv_setspeed(struct train *tr, uint8_t speed)
{
    assert(speed > 0 && speed < 15);
    tr->speed = speed;
    if (tr->state == TRAIN_ENROUTE)
        tcmux_train_speed(&tr->tcmux, tr->train_id, speed);
}

static void
trainsrv_moveto(struct train *tr, const struct track_node *dest)
{
    if (tr->state != TRAIN_STOPPED)
        return;

    /* Get path to follow */
    tr->path_len = track_pathfind(tr->track, tr->ahead, dest, tr->path);
    if (tr->path_len <= 1)
        return;

    /* Watch for last sensor on path. */
    tr->path_next = 1;
    if (!trainsrv_expect_next_sensor(tr))
        return;

    /* Switch all switches and go. */
    trainsrv_switch_upto_sensnext(tr);
    tcmux_train_speed(&tr->tcmux, tr->train_id, tr->speed);
    tr->state = TRAIN_ENROUTE;
}

static void
trainsrv_stop(struct train *tr)
{
    tcmux_train_speed(&tr->tcmux, tr->train_id, 0);
    tr->state = TRAIN_STOPPED;
}

static void
trainsrv_sensor_disoriented(struct train *tr)
{
    trainsrv_expect_sensor(tr, -1, -1);
    tcmux_train_speed(&tr->tcmux, tr->train_id, TRAIN_CRAWLSPEED);
    tr->state = TRAIN_ORIENTING;
}

static void
trainsrv_sensor_orienting(struct train *tr, sensors_t sensors[SENSOR_MODULES])
{
    int i, sensor;
    const struct track_node *sensnode;
    const struct track_edge *ahead;
    tcmux_train_speed(&tr->tcmux, tr->train_id, 0);
    for (i = 0; i < SENSOR_MODULES; i++) {
        if (sensors[i] != 0)
            break;
    }
    assert(i != SENSOR_MODULES);
    sensor       = i * SENSORS_PER_MODULE + ctz16(sensors[i]);
    sensnode     = &tr->track->nodes[sensor];
    ahead        = &sensnode->edge[TRACK_EDGE_AHEAD];
    tr->ahead    = ahead->dest;
    tr->ahead_mm = ahead->len_mm - TRAIN_FRONT_OFFS_MM;
    /* TODO figure out behind */
    tr->state    = TRAIN_STOPPED;
}

static void
trainsrv_sensor_enroute(struct train *tr, sensors_t sensors[SENSOR_MODULES])
{
    (void)sensors;
    tr->path_next = tr->path_sensnext + 1;
    if (trainsrv_expect_next_sensor(tr))
        trainsrv_switch_upto_sensnext(tr);
    else
        trainsrv_stop(tr);
}

static void
trainsrv_sensor(struct train *tr, sensors_t sensors[SENSOR_MODULES])
{
    tr->sensor_rdy = true;
    switch (tr->state) {
    case TRAIN_DISORIENTED:
        trainsrv_sensor_disoriented(tr);
        break;
    case TRAIN_ORIENTING:
        trainsrv_sensor_orienting(tr, sensors);
        break;
    case TRAIN_ENROUTE:
        trainsrv_sensor_enroute(tr, sensors);
        break;
    default:
        panic("unexpected sensor reading in train state %d", tr->state);
    }
}

static void
trainsrv_expect_sensor(struct train *tr, int sensor, int timeout)
{
    struct sensor_reply reply;
    int i, rc;
    assert(tr->sensor_rdy);
    reply.timeout = timeout;
    if (sensor == -1) {
        for (i = 0; i < SENSOR_MODULES; i++)
            reply.sensors[i] = (sensors_t)-1;
    } else {
        int module;
        assert(sensor >= 0 && sensor < SENSOR_MODULES * SENSORS_PER_MODULE);
        for (i = 0; i < SENSOR_MODULES; i++)
            reply.sensors[i] = 0;
        module  = sensor / SENSORS_PER_MODULE;
        sensor %= SENSORS_PER_MODULE;
        reply.sensors[module] = 1 << sensor;
    }
    rc = Reply(tr->sensor_tid, &reply, sizeof (reply));
    assertv(rc, rc == 0);
    tr->sensor_rdy = false;
}

static void
trainsrv_switch_upto_sensnext(struct train *tr)
{
    int i, sensors;
    sensors = 0;
    for (i = tr->path_next; i < tr->path_len && sensors < 2; i++) {
        bool curved;
        if (tr->path[i]->type == TRACK_NODE_SENSOR)
            sensors++;
        if (tr->path[i]->type != TRACK_NODE_BRANCH)
            continue;
        curved = tr->path[i]->edge[TRACK_EDGE_CURVED].dest == tr->path[i + 1];
        tcmux_switch_curve(&tr->tcmux, tr->path[i]->num, curved);
    }
}

static bool
trainsrv_expect_next_sensor(struct train *tr)
{
    int i;
    for (i = tr->path_next; i < tr->path_len; i++) {
        if (tr->path[i]->type == TRACK_NODE_SENSOR)
            break;
    }
    if (i == tr->path_len)
        return false;

    tr->path_sensnext = i;
    trainsrv_expect_sensor(tr, tr->path[i]->num, -1);
    return true;
}

static void
trainsrv_sensor_listen(void)
{
    struct sensorctx sens;
    struct trainmsg msg;
    struct sensor_reply reply;
    int rc, rplylen;
    tid_t train;

    train = MyParentTid();
    sensorctx_init(&sens);

    rc = SENSOR_TIMEOUT;
    for (;;) {
        msg.type = TRAINMSG_SENSOR;
        if (rc == SENSOR_TIMEOUT)
            memset(msg.sensors, '\0', sizeof (msg.sensors));
        else
            memcpy(msg.sensors, reply.sensors, sizeof (msg.sensors));
        rplylen = Send(train, &msg, sizeof (msg), &reply, sizeof (reply));
        assertv(rplylen, rplylen == sizeof (reply));
        rc = sensor_wait(&sens, reply.sensors, reply.timeout);
    }
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
