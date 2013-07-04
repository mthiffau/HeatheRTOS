#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "bithack.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "u_tid.h"
#include "train_srv.h"

#include "calib.h"

#include "xdef.h"
#include "xarg.h"
#include "xassert.h"
#include "xmemcpy.h"
#include "ringbuf.h"

#include "u_syscall.h"
#include "ns.h"
#include "clock_srv.h"
#include "tcmux_srv.h"
#include "sensor_srv.h"
#include "dbglog_srv.h"

#define TRAIN_MINSPEED          8
#define TRAIN_MAXSPEED          14
#define TRAIN_DEFSPEED          TRAIN_MINSPEED
#define TRAIN_CRAWLSPEED        2

/* FIXME these should be looked-up per train */
#define TRAIN_FRONT_OFFS_MM     30
#define TRAIN_BACK_OFFS_MM      145

enum {
    TRAINMSG_SETSPEED,
    TRAINMSG_MOVETO,
    TRAINMSG_STOP,
    TRAINMSG_SENSOR,
    TRAINMSG_TIMER,
    TRAINMSG_VCALIB,
    TRAINMSG_STOPCALIB,
    TRAINMSG_ORIENT,
    TRAINMSG_ESTIMATE
};

enum {
    TRAIN_DISORIENTED,
    TRAIN_ORIENTING,
    TRAIN_PRE_VCALIB,
    TRAIN_VCALIB,
    TRAIN_PRE_STOPCALIB,
    TRAIN_STOPCALIB,
    TRAIN_STOPPED,
    TRAIN_ENROUTE,
};

struct trainmsg {
    int type;
    int time; /* sensors, timer only */
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

struct trainest_reply {
    uint8_t train_id;
    int     ahead_edge_id;
    int     ahead_mm;
    int     lastsens_node_id;
    int     err_mm;
};

struct train {
    /* Server handles */
    struct clkctx             clock;
    struct tcmuxctx           tcmux;
    struct dbglogctx          dbglog;

    /* General train state */
    int                       state;
    const struct track_graph *track;
    uint8_t                   train_id;
    uint8_t                   speed;

    /* Velocity calibration */
    struct calib              calib;
    int                       last_sensor_ix;
    int                       last_sensor_time;
    int                       ignore_time, lap_count;
    uint8_t                   calib_speed;
    bool                      calib_decel;

    /* Sensor bookkeeping */
    tid_t                     sensor_tid;
    bool                      sensor_rdy;

    /* Timer */
    tid_t                     timer_tid;

    /* Position estimate */
    const struct track_edge  *ahead, *behind;
    int                       ahead_mm, behind_mm;
    int                       pos_time;
    tid_t                     estimate_client;

    /* Path to follow */
    const struct track_node  *path[TRACK_NODES_MAX];
    int                       path_len, path_next, path_sensnext;
};

static void trainsrv_init(struct train *tr, struct traincfg *cfg);
static void trainsrv_setspeed(struct train *tr, uint8_t speed);
static void trainsrv_moveto(struct train *tr, const struct track_node *dest);
static void trainsrv_stop(struct train *tr);
static void trainsrv_vcalib(struct train *tr);
static void trainsrv_stopcalib(struct train *tr, uint8_t speed);
static void trainsrv_moveto_calib(struct train *tr);
static void trainsrv_calibspeed(struct train *tr, uint8_t speed);
static void trainsrv_sensor_disoriented(struct train *tr);
static void trainsrv_sensor_orienting(
    struct train *tr, sensors_t sensors[SENSOR_MODULES]);
static void trainsrv_sensor_vcalib(struct train *tr, int time);
static void trainsrv_sensor_stopcalib(struct train *tr);
static void trainsrv_sensor_pre_vcalib(struct train *tr);
static void trainsrv_sensor_pre_stopcalib(struct train *tr);
static void trainsrv_calib_setup(struct train *tr);
static void trainsrv_sensor_enroute(
    struct train *tr, sensors_t sensors[SENSOR_MODULES], int time);
static void trainsrv_sensor(
    struct train *tr, sensors_t sensors[SENSOR_MODULES], int time);
static void trainsrv_timer(struct train *tr, int time);
static void trainsrv_expect_sensor(struct train *tr, int sensor, int timeout);
static bool trainsrv_expect_next_sensor(struct train *tr);
static void trainsrv_switch_upto_sensnext(struct train *tr);
static void trainsrv_send_estimate(struct train *tr);

static void trainsrv_sensor_listen(void);
static void trainsrv_timer_listen(void);
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
            trainsrv_sensor(&tr, msg.sensors, msg.time);
            break;
        case TRAINMSG_TIMER:
            assert(client == tr.timer_tid);
            trainsrv_empty_reply(client);
            trainsrv_timer(&tr, msg.time);
            break;
        case TRAINMSG_VCALIB:
            trainsrv_empty_reply(client);
            trainsrv_vcalib(&tr);
            break;
        case TRAINMSG_STOPCALIB:
            trainsrv_empty_reply(client);
            trainsrv_stopcalib(&tr, msg.speed);
            break;
        case TRAINMSG_ORIENT:
            trainsrv_empty_reply(client);
            trainsrv_sensor_disoriented(&tr);
            break;
        case TRAINMSG_ESTIMATE:
            assert(tr.estimate_client < 0);
            tr.estimate_client = client;
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
    tr->pos_time = -1;
    tr->estimate_client = -1;

    /* take initial calibration */
    memcpy(
        &tr->calib,
        &calib_initial[cfg->train_id][cfg->track_id],
        sizeof (tr->calib));

    tr->sensor_rdy = false;
    tr->sensor_tid = Create(PRIORITY_TRAIN - 1, &trainsrv_sensor_listen);
    assert(tr->sensor_tid >= 0);

    tr->timer_tid = Create(PRIORITY_TRAIN - 1, &trainsrv_timer_listen);
    assert(tr->timer_tid >= 0);

    clkctx_init(&tr->clock);
    tcmuxctx_init(&tr->tcmux);
    dbglogctx_init(&tr->dbglog);

    /* log starting calibration */
    dbglog(&tr->dbglog, "train%d track%d initial velocities: %d %d %d %d %d",
        cfg->train_id,
        cfg->track_id,
        tr->calib.vel_mmps[8],
        tr->calib.vel_mmps[9],
        tr->calib.vel_mmps[10],
        tr->calib.vel_mmps[11],
        tr->calib.vel_mmps[12]);
    dbglog(&tr->dbglog, "train%d track%d stopping distances: %d %d %d %d %d",
        cfg->train_id,
        cfg->track_id,
        tr->calib.stop_dist[8],
        tr->calib.stop_dist[9],
        tr->calib.stop_dist[10],
        tr->calib.stop_dist[11],
        tr->calib.stop_dist[12]);
}

static void
trainsrv_setspeed(struct train *tr, uint8_t speed)
{
    assert(speed > 0 && speed < 15);
    if (tr->state != TRAIN_STOPPED && tr->state != TRAIN_ENROUTE)
        return; /* ignore */
    if (speed < TRAIN_MINSPEED)
        speed = TRAIN_MINSPEED;
    if (speed > TRAIN_MAXSPEED)
        speed = TRAIN_MAXSPEED;
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
    tr->path_len = track_pathfind(tr->track, tr->ahead->dest, dest, tr->path);
    if (tr->path_len <= 1)
        return;

    /* Watch for last sensor on path. */
    tr->path_next = 0;
    if (!trainsrv_expect_next_sensor(tr))
        return;

    /* Switch all switches and go. */
    trainsrv_switch_upto_sensnext(tr);
    tcmux_train_speed(&tr->tcmux, tr->train_id, tr->speed);
    tr->state = TRAIN_ENROUTE;
}

static void
trainsrv_vcalib(struct train *tr)
{
    if (tr->state != TRAIN_STOPPED)
        return; /* ignore */

    /* start calibrating */
    tr->state = TRAIN_PRE_VCALIB;
    trainsrv_moveto_calib(tr);
}

static void
trainsrv_stopcalib(struct train *tr, uint8_t speed)
{
    if (tr->state != TRAIN_STOPPED)
        return; /* ignore */

    /* start calibrating */
    tr->calib_speed = speed;
    tr->state = TRAIN_PRE_STOPCALIB;
    trainsrv_moveto_calib(tr);
}

static void
trainsrv_moveto_calib(struct train *tr)
{
    int i;
    tr->path_len = track_pathfind(
        tr->track,
        tr->ahead->dest,
        tr->track->calib_sensors[0],
        tr->path);
    assert(tr->path_len >= 1);

    /* Set all switches on path to destination */
    for (i = 0; i < tr->path_len - 1; i++) {
        bool curved;
        if (tr->path[i]->type != TRACK_NODE_BRANCH)
            continue;
        curved = tr->path[i]->edge[TRACK_EDGE_CURVED].dest == tr->path[i + 1];
        tcmux_switch_curve(&tr->tcmux, tr->path[i]->num, curved);
    }

    trainsrv_expect_sensor(tr, tr->track->calib_sensors[0]->num, -1);
    tcmux_train_speed(&tr->tcmux, tr->train_id, TRAIN_DEFSPEED);
}

static void
trainsrv_stop(struct train *tr)
{
    tcmux_train_speed(&tr->tcmux, tr->train_id, 0);
    tr->state = TRAIN_DISORIENTED; /* FIXME */
}

static void
trainsrv_calibspeed(struct train *tr, uint8_t speed)
{
    tr->state          = TRAIN_VCALIB;
    tr->calib_speed    = speed;
    tr->last_sensor_ix = 0;
    tr->ignore_time    = 2;
    tr->lap_count      = 2;
    tr->calib.vel_mmps[speed] = -1;
    trainsrv_expect_sensor(tr, tr->track->calib_sensors[1]->num, -1);
    tcmux_train_speed(&tr->tcmux, tr->train_id, tr->calib_speed);
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
    tcmux_train_speed(&tr->tcmux, tr->train_id, 0);
    for (i = 0; i < SENSOR_MODULES; i++) {
        if (sensors[i] != 0)
            break;
    }
    assert(i != SENSOR_MODULES);
    sensor       = i * SENSORS_PER_MODULE + ctz16(sensors[i]);
    sensnode     = &tr->track->nodes[sensor];
    tr->ahead    = &sensnode->edge[TRACK_EDGE_AHEAD];
    tr->ahead_mm = tr->ahead->len_mm - TRAIN_FRONT_OFFS_MM;
    /* TODO figure out behind */
    tr->pos_time = Time(&tr->clock);
    trainsrv_send_estimate(tr);

    tr->state = TRAIN_STOPPED;
}

static void
trainsrv_sensor_vcalib(struct train *tr, int time)
{
    int cur_sensor_ix = (tr->last_sensor_ix + 1) % tr->track->n_calib_sensors;
    if (tr->ignore_time > 0) {
        tr->ignore_time--;
    } else {
        int  x    = tr->track->calib_dists[cur_sensor_ix];
        int  dt   = time - tr->last_sensor_time;
        int  v    = 100 * (x + dt / 2) / dt;
        int *calv = &tr->calib.vel_mmps[tr->speed];
        if (*calv < 0)
            *calv = v;
        else
            *calv = (v + 7 * (*calv) + 4) / 8; /* +4 to round */
    }

    tr->last_sensor_ix   = cur_sensor_ix;
    tr->last_sensor_time = time;

    if (cur_sensor_ix == 0 && --tr->lap_count == 0) {
        dbglog(&tr->dbglog, "train%d speed %d%s = %d mm/s",
            tr->train_id,
            tr->calib_speed,
            tr->calib_decel ? "'" : "",
            tr->calib.vel_mmps[tr->speed]);
        if (tr->calib_decel && tr->calib_speed == TRAIN_MINSPEED) {
            tcmux_train_speed(&tr->tcmux, tr->train_id, 0);
            /* TODO FIXME DO SOMETHING SENSIBLE HERE */
            tr->state = TRAIN_DISORIENTED;
        } else {
            int new_speed = tr->calib_speed + (tr->calib_decel ? -1 : 1);
            if (new_speed > TRAIN_MAXSPEED) {
                new_speed -= 2;
                tr->calib_decel = true;
            }
            trainsrv_calibspeed(tr, new_speed);
        }
    } else {
        int next = cur_sensor_ix + 1;
        next    %= tr->track->n_calib_sensors;
        trainsrv_expect_sensor(tr, tr->track->calib_sensors[next]->num, -1);
    }
}

static void
trainsrv_sensor_pre_vcalib(struct train *tr)
{
    trainsrv_calib_setup(tr);
    trainsrv_calibspeed(tr, TRAIN_MINSPEED);
    tr->calib_decel = false;
}

static void
trainsrv_sensor_pre_stopcalib(struct train *tr)
{
    trainsrv_calib_setup(tr);
    trainsrv_expect_sensor(tr, tr->track->calib_sensors[0]->num, -1);
    tcmux_train_speed(&tr->tcmux, tr->train_id, tr->calib_speed);
    tr->state = TRAIN_STOPCALIB;
}

static void
trainsrv_calib_setup(struct train *tr)
{
    int i;
    tcmux_train_speed(&tr->tcmux, tr->train_id, 0);
    for (i = 0; i < tr->track->n_calib_switches - 1; i++) {
        tcmux_switch_curve(
            &tr->tcmux,
            tr->track->calib_switches[i]->num,
            tr->track->calib_curved[i]);
    }

    tcmux_switch_curve_sync(
        &tr->tcmux,
        tr->track->calib_switches[i]->num,
        tr->track->calib_curved[i]);
}

static void
trainsrv_sensor_stopcalib(struct train *tr)
{
    tcmux_train_speed(&tr->tcmux, tr->train_id, 0);
    tr->state = TRAIN_DISORIENTED;
}


static void
trainsrv_sensor_enroute(
    struct train *tr,
    sensors_t sensors[SENSOR_MODULES],
    int time)
{
    const struct track_node *sens;
    (void)sensors;

    tr->path_next = tr->path_sensnext + 1;
    sens = tr->path[tr->path_sensnext];
    assert(sens->type == TRACK_NODE_SENSOR);

    tr->ahead    = &sens->edge[TRACK_EDGE_AHEAD];
    tr->ahead_mm = tr->ahead->len_mm;
    tr->pos_time = time;
    trainsrv_send_estimate(tr);

    if (trainsrv_expect_next_sensor(tr))
        trainsrv_switch_upto_sensnext(tr);
    else
        trainsrv_stop(tr);
}

static void
trainsrv_sensor(struct train *tr, sensors_t sensors[SENSOR_MODULES], int time)
{
    /*const struct track_node *sensor;
    sensor1_t sensor1;*/

    tr->sensor_rdy = true;
#if 0 /* FIXME */
    sensor1 = sensors_scan(sensors);
    if (sensor1 == SENSOR1_INVALID)
        sensor = NULL;
    else
        sensor = &tr->track->nodes[sensor1];

    dbglog(&tr->dbglog, "train%d hit %s",
        tr->train_id,
        sensor == NULL ? "none" : sensor->name);

    if (tr->state != TRAIN_ENROUTE)
        goto skip_last_sensor;
    if (tr->last_sensor == NULL) {
        tr->last_sensor = &tr->path[0];
        while (*tr->last_sensor != sensor)
            tr->last_sensor++;
        tr->vel_mmps = 0;
    } else {
        dist = 0;
        while (*tr->last_sensor != sensor) {
            int i, edges = 1;
            if ((*tr->last_sensor)->type == TRACK_NODE_BRANCH)
                edges = 2;
            for (i = 0; i < edges; i++) {
                if ((*tr->last_sensor)->edge[i].dest == *(tr->last_sensor + 1))
                    break;
            }
            assert(i < edges);
            dist += (*tr->last_sensor)->edge[i].len_mm;
            tr->last_sensor++;
        }
        int dt = time - tr->last_sensor_time;
        int v  = 100 * (dist + dt / 2) / dt;
        tr->vel_mmps = (tr->vel_mmps >> 1) + (v >> 1);
        dbglog(&tr->dbglog,
            "train%d velocity=%dmm/s, %dmm/s",
            tr->train_id,
            tr->vel_mmps,
            v);
        }
    }

    tr->last_sensor_time = time;
skip_last_sensor:
#endif

    switch (tr->state) {
    case TRAIN_DISORIENTED:
        trainsrv_sensor_disoriented(tr);
        break;
    case TRAIN_ORIENTING:
        trainsrv_sensor_orienting(tr, sensors);
        break;
    case TRAIN_PRE_VCALIB:
        trainsrv_sensor_pre_vcalib(tr);
        break;
    case TRAIN_VCALIB:
        trainsrv_sensor_vcalib(tr, time);
        break;
    case TRAIN_PRE_STOPCALIB:
        trainsrv_sensor_pre_stopcalib(tr);
        break;
    case TRAIN_STOPCALIB:
        trainsrv_sensor_stopcalib(tr);
        break;
    case TRAIN_ENROUTE:
        trainsrv_sensor_enroute(tr, sensors, time);
        break;
    default:
        panic("unexpected sensor reading in train state %d", tr->state);
    }
}

static void
trainsrv_timer(struct train *tr, int time)
{
    int dist;

    if (tr->state == TRAIN_STOPPED) {
        tr->pos_time = time;
        return;
    }

    if (tr->state != TRAIN_ENROUTE)
        return;

    /* +50 to round */
    dist  = tr->calib.vel_mmps[tr->speed] * (time - tr->pos_time) + 50;
    dist /= 100;
    tr->pos_time = time;

    tr->ahead_mm -= dist;
    while (tr->ahead_mm < 0) {
        /* TODO: should query switch server here FIXME FIXME BAD HACK HACK XXX */
        const struct track_node *next = tr->ahead->dest;
        const struct track_node *after_next;
        int i, edges;
        assert(tr->path[tr->path_next] == next);
        if (tr->path_next >= tr->path_len - 1) {
            trainsrv_stop(tr);
            return;
        }
        after_next = tr->path[++tr->path_next];
        assert(next->type != TRACK_NODE_EXIT);
        edges = next->type == TRACK_NODE_BRANCH ? 2 : 1;
        for (i = 0; i < edges; i++) {
            if (next->edge[i].dest == after_next)
                break;
        }
        assert(i < edges);
        tr->ahead     = &next->edge[i];
        tr->ahead_mm += tr->ahead->len_mm;
    }

    trainsrv_send_estimate(tr);

    /*dbglog(&tr->dbglog, "train%d at %s - %dmm, %d ticks",
        tr->train_id,
        tr->ahead->dest->name,
        tr->ahead_mm,
        time);*/
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
        dbglog(&tr->dbglog, "setting switch %d to %s",
            tr->path[i]->num,
            curved ? "curved" : "straight");
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
trainsrv_send_estimate(struct train *tr)
{
    struct trainest_reply est;
    int rc;

    if (tr->estimate_client < 0)
        return;

    est.train_id         = tr->train_id;
    est.ahead_edge_id    = (tr->ahead->src - tr->track->nodes) << 1;
    est.ahead_edge_id   |= tr->ahead - tr->ahead->src->edge;
    est.ahead_mm         = tr->ahead_mm;
    est.lastsens_node_id = 0; /* FIXME */
    est.err_mm           = 1000; /* FIXME */

    rc = Reply(tr->estimate_client, &est, sizeof (est));
    assertv(rc, rc == 0);

    tr->estimate_client = -1;
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
    msg.time = 0;
    for (;;) {
        msg.type = TRAINMSG_SENSOR;
        if (rc == SENSOR_TIMEOUT)
            memset(msg.sensors, '\0', sizeof (msg.sensors));
        else
            memcpy(msg.sensors, reply.sensors, sizeof (msg.sensors));
        rplylen = Send(train, &msg, sizeof (msg), &reply, sizeof (reply));
        assertv(rplylen, rplylen == sizeof (reply));
        rc = sensor_wait(&sens, reply.sensors, reply.timeout, &msg.time);
    }
}

static void
trainsrv_timer_listen(void)
{
    struct clkctx clock;
    struct trainmsg msg;
    tid_t train;

    train = MyParentTid();
    clkctx_init(&clock);

    msg.type = TRAINMSG_TIMER;
    msg.time = Time(&clock);
    for (;;) {
        int rplylen;
        msg.time += 5;
        DelayUntil(&clock, msg.time);
        rplylen = Send(train, &msg, sizeof (msg), NULL, 0);
        assertv(rplylen, rplylen == 0);
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

void
train_vcalib(struct trainctx *ctx)
{
    struct trainmsg msg;
    int rplylen;
    msg.type = TRAINMSG_VCALIB;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

void
train_stopcalib(struct trainctx *ctx, uint8_t speed)
{
    struct trainmsg msg;
    int rplylen;
    msg.type  = TRAINMSG_STOPCALIB;
    msg.speed = speed;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

void
train_orient(struct trainctx *ctx)
{
    struct trainmsg msg;
    int rplylen;
    msg.type = TRAINMSG_ORIENT;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

void
train_estimate(struct trainctx *ctx, struct trainest *est_out)
{
    struct trainmsg msg;
    struct trainest_reply est;
    int rplylen, ahead_src, ahead;
    msg.type = TRAINMSG_ESTIMATE;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), &est, sizeof (est));
    assertv(rplylen, rplylen == sizeof (est));
    ahead_src = est.ahead_edge_id / 2;
    ahead     = est.ahead_edge_id % 2;
    est_out->train_id = est.train_id;
    est_out->ahead    = &ctx->track->nodes[ahead_src].edge[ahead];
    est_out->ahead_mm = est.ahead_mm;
    est_out->lastsens = &ctx->track->nodes[est.lastsens_node_id];
    est_out->err_mm   = est.err_mm;
}

static void
trainsrv_fmt_name(uint8_t train_id, char buf[32])
{
    struct ringbuf rbuf;
    rbuf_init(&rbuf, buf, 32);
    rbuf_printf(&rbuf, "train%d", train_id);
    rbuf_putc(&rbuf, '\0');
}
