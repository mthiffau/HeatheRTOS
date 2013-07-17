#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "bithack.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "u_tid.h"
#include "switch_srv.h"
#include "track_pt.h"
#include "train_srv.h"

#include "xarg.h"
#include "xassert.h"
#include "xmemcpy.h"
#include "array_size.h"
#include "ringbuf.h"
#include "pqueue.h"

#include "u_syscall.h"
#include "ns.h"
#include "clock_srv.h"
#include "tcmux_srv.h"
#include "sensor_srv.h"
#include "tracksel_srv.h"
#include "calib_srv.h"
#include "dbglog_srv.h"

#define TRAIN_TIMER_TICKS       1

/* FIXME these should be looked-up per train */
#define TRAIN_FRONT_OFFS_UM     30000
#define TRAIN_BACK_OFFS_UM      145000
#define TRAIN_LENGTH_UM         214000

#define PCTRL_ACCEL_SENSORS     3
#define PCTRL_STOP_TICKS        400

#define TRAIN_SENSOR_OVERESTIMATE_UM    (80 * 1000) // FIXME more like 4cm
#define TRAIN_SWITCH_AHEAD_TICKS        50
#define TRAIN_MERGE_OFFSET_UM           100000 // 10cm

enum {
    TRAINMSG_SETSPEED,
    TRAINMSG_MOVETO,
    TRAINMSG_STOP,
    TRAINMSG_SENSOR,
    TRAINMSG_SENSOR_TIMEOUT,
    TRAINMSG_TIMER,
    TRAINMSG_ESTIMATE
};

enum {
    TRAIN_DISORIENTED,
    TRAIN_ORIENTING,
    TRAIN_RUNNING,
};

enum {
    PCTRL_STOPPED,
    PCTRL_ACCEL,
    PCTRL_STOPPING,
    PCTRL_CRUISE,
};

struct trainmsg {
    int type;
    int time; /* sensors, timer only */
    union {
        uint8_t         speed;
        struct track_pt dest;
        sensors_t       sensors[SENSOR_MODULES];
    };
};

struct sensor_reply {
    sensors_t sensors[SENSOR_MODULES];
    int       timeout;
};

struct train_pctrl {
    int                       state;
    struct track_pt           ahead, behind;
    int                       pos_time, accel_start;
    int                       stop_ticks, stop_um;
    bool                      reversed;
    const struct track_node  *lastsens;
    int                       err_um;
    bool                      updated;
};

struct train {
    /* Server handles */
    struct clkctx             clock;
    struct tcmuxctx           tcmux;
    struct switchctx          switches;
    struct dbglogctx          dbglog;

    /* General train state */
    int                       state;
    const struct track_graph *track;
    uint8_t                   train_id;
    uint8_t                   speed;

    /* Calibration data */
    struct calib              calib;

    /* Sensor bookkeeping */
    tid_t                     sensor_tid;
    bool                      sensor_rdy;
    sensors_t                 sensor_mask[SENSOR_MODULES];
    struct pqueue             sensor_times;
    struct pqueue_node        sensor_times_nodes[SENSOR_MODULES * SENSORS_PER_MODULE];

    /* Timer */
    tid_t                     timer_tid;

    /* Position estimate */
    struct train_pctrl        pctrl;
    tid_t                     estimate_client;

    /* Path to follow */
    struct track_path         path;
    int                       path_swnext;
    bool                      reverse_ok;
};

static void trainsrv_init(struct train *tr, struct traincfg *cfg);
static void trainsrv_setspeed(struct train *tr, uint8_t speed);
static void trainsrv_swnext_init(struct train *tr);
static void trainsrv_swnext_advance(struct train *tr);
static void trainsrv_moveto(struct train *tr, struct track_pt dest);
static void trainsrv_stop(struct train *tr);
static void trainsrv_sensor_orienting(struct train *tr, track_node_t sens);
static void trainsrv_sensor_running(struct train *tr, track_node_t sens, int time);
static void trainsrv_sensor(
    struct train *tr, sensors_t sensors[SENSOR_MODULES], int time);
static void trainsrv_start_orienting(struct train *tr);
static void trainsrv_sensor_timeout(struct train *tr, int time);
static int  trainsrv_accel_pos(struct train *tr, int t);
static int  trainsrv_predict_dist_um(struct train *tr, int when);
static void trainsrv_pctrl_advance_ticks(struct train *tr, int now);
static void trainsrv_pctrl_advance_um(struct train *tr, int dist_um);
static void trainsrv_timer(struct train *tr, int time);
static void trainsrv_expect_sensor(struct train *tr, track_node_t sens);
static void trainsrv_tryrequest_sensor(struct train *tr);
static void trainsrv_pctrl_check_update(struct train *tr);
static void trainsrv_pctrl_switch_turnouts(struct train *tr, bool all);
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
            trainsrv_moveto(&tr, msg.dest);
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
        case TRAINMSG_SENSOR_TIMEOUT:
            assert(client == tr.sensor_tid);
            assert(!tr.sensor_rdy);
            trainsrv_sensor_timeout(&tr, msg.time);
            break;
        case TRAINMSG_TIMER:
            assert(client == tr.timer_tid);
            trainsrv_empty_reply(client);
            trainsrv_timer(&tr, msg.time);
            break;
        case TRAINMSG_ESTIMATE:
            assert(tr.estimate_client < 0);
            tr.estimate_client = client;
            break;
        default:
            panic("invalid train message type %d", msg.type);
        }
        trainsrv_tryrequest_sensor(&tr);
        trainsrv_pctrl_check_update(&tr);
    }
}

static void
trainsrv_init(struct train *tr, struct traincfg *cfg)
{
    unsigned i;

    tr->state    = TRAIN_DISORIENTED;
    tr->track    = tracksel_ask();
    tr->train_id = cfg->train_id;
    tr->speed    = TRAIN_DEFSPEED;

    tr->pctrl.state = PCTRL_STOPPED;
    tr->pctrl.pos_time = -1;
    tr->pctrl.reversed = false;
    tr->pctrl.lastsens = NULL;
    tr->pctrl.updated  = false;
    tr->estimate_client = -1;

    /* take initial calibration */
    calib_get(tr->train_id, &tr->calib);

    /* Adjust stopping distance values. They're too long by
     * SENSOR_AVG_DELAY_TICKS worth of distance at cruising velocity. */
    for (i = 0; i < ARRAY_SIZE(tr->calib.vel_umpt); i++)
        tr->calib.stop_um[i] -= tr->calib.vel_umpt[i] * SENSOR_AVG_DELAY_TICKS;

    tr->sensor_rdy = false;
    pqueue_init(
        &tr->sensor_times,
        ARRAY_SIZE(tr->sensor_times_nodes),
        tr->sensor_times_nodes);
    for (i = 0; i < ARRAY_SIZE(tr->sensor_mask); i++)
        tr->sensor_mask[i] = 0;
    tr->sensor_tid = Create(PRIORITY_TRAIN - 1, &trainsrv_sensor_listen);
    assert(tr->sensor_tid >= 0);

    tr->timer_tid = Create(PRIORITY_TRAIN - 1, &trainsrv_timer_listen);
    assert(tr->timer_tid >= 0);

    clkctx_init(&tr->clock);
    tcmuxctx_init(&tr->tcmux);
    switchctx_init(&tr->switches);
    dbglogctx_init(&tr->dbglog);

    /* log starting calibration */
    dbglog(&tr->dbglog, "train%d track %s initial velocities (um/tick): %d %d %d %d %d",
        cfg->train_id,
        tr->track->name,
        tr->calib.vel_umpt[8],
        tr->calib.vel_umpt[9],
        tr->calib.vel_umpt[10],
        tr->calib.vel_umpt[11],
        tr->calib.vel_umpt[12]);
    dbglog(&tr->dbglog, "train%d track %s stopping distances (um): %d %d %d %d %d",
        cfg->train_id,
        tr->track->name,
        tr->calib.stop_um[8],
        tr->calib.stop_um[9],
        tr->calib.stop_um[10],
        tr->calib.stop_um[11],
        tr->calib.stop_um[12]);
}

static void
trainsrv_setspeed(struct train *tr, uint8_t speed)
{
    assert(speed > 0 && speed < 15);
    if (tr->state != TRAIN_RUNNING)
        return; /* ignore */
    if (speed < TRAIN_MINSPEED)
        speed = TRAIN_MINSPEED;
    if (speed > TRAIN_MAXSPEED)
        speed = TRAIN_MAXSPEED;
    tr->speed = speed;
    if (tr->pctrl.state == PCTRL_CRUISE) {
        tcmux_train_speed(&tr->tcmux, tr->train_id, speed);
        tr->pctrl.state = PCTRL_ACCEL;
    }
}

static void
trainsrv_swnext_init(struct train *tr)
{
    tr->path_swnext = -1;
    trainsrv_swnext_advance(tr);
}

static void
trainsrv_swnext_advance(struct train *tr)
{
    if (tr->path_swnext >= 0 && (unsigned)tr->path_swnext >= tr->path.hops)
        return;
    tr->path_swnext++;
    if (tr->path_swnext == 0
        && tr->path.edges[tr->path_swnext]->src->type == TRACK_NODE_MERGE)
        tr->path_swnext++;
    while ((unsigned)tr->path_swnext < tr->path.hops) {
        int type = tr->path.edges[tr->path_swnext]->src->type;
        if (type == TRACK_NODE_MERGE || type == TRACK_NODE_BRANCH)
            break;
        tr->path_swnext++;
    }
}

static void
trainsrv_moveto(struct train *tr, struct track_pt dest)
{
    struct track_pt path_starts[2], path_dests[2];
    int rc, n_starts;
    if (tr->state != TRAIN_RUNNING || tr->pctrl.state != PCTRL_STOPPED)
        return;

    /* Get path to follow */
    n_starts = 0;
    path_starts[n_starts++] = tr->pctrl.ahead;
    if (tr->reverse_ok) {
        path_starts[n_starts] = tr->pctrl.behind;
        track_pt_reverse(&path_starts[n_starts]);
        n_starts++;
    }

    path_dests[0] = dest;
    path_dests[1] = path_dests[0];
    track_pt_reverse(&path_dests[1]);

    rc = track_pathfind(
        tr->track,
        path_starts,
        n_starts,
        path_dests,
        ARRAY_SIZE(path_dests),
        &tr->path);

    if (rc < 0 || tr->path.hops == 0)
        return;

    if (tr->reverse_ok && tr->path.edges[0] == path_starts[1].edge) {
        tcmux_train_speed(&tr->tcmux, tr->train_id, 15);
        tr->pctrl.ahead  = path_starts[1];
        tr->pctrl.behind = path_starts[0];
        track_pt_reverse(&tr->pctrl.behind);
        tr->pctrl.reversed = !tr->pctrl.reversed;
    }

    trainsrv_swnext_init(tr);
    tr->pctrl.pos_time = Time(&tr->clock);
    tr->pctrl.updated  = true;

    tcmux_train_speed(&tr->tcmux, tr->train_id, tr->speed);
    dbglog(&tr->dbglog, "train%d sending speed %d", tr->train_id, tr->speed);
    /* already TRAIN_RUNNING */
    tr->pctrl.state = PCTRL_ACCEL;
    tr->pctrl.accel_start = tr->pctrl.pos_time + tr->calib.accel_delay;
}

static void
trainsrv_stop(struct train *tr)
{
    bool cruise;
    if (tr->state != TRAIN_RUNNING)
        return; /* ignore */
    if (tr->pctrl.state == PCTRL_STOPPING || tr->pctrl.state == PCTRL_STOPPED)
        return; /* ignore */
    tcmux_train_speed(&tr->tcmux, tr->train_id, 0);
    cruise = tr->pctrl.state == PCTRL_CRUISE;
    tr->pctrl.state      = PCTRL_STOPPING;
    tr->pctrl.stop_ticks = PCTRL_STOP_TICKS;
    tr->pctrl.stop_um    = cruise ? tr->calib.stop_um[tr->speed] : -1;

    /* Switch all remaining switches on path, since estimation stops.
     * FIXME this goes away with better estimations */
    trainsrv_pctrl_switch_turnouts(tr, true);
}

static void
trainsrv_determine_reverse_ok(struct train *tr)
{
    track_edge_t edge = tr->pctrl.behind.edge;
    while (edge != tr->pctrl.ahead.edge) {
        int ix;
        if (edge->dest->type == TRACK_NODE_MERGE) {
            bool want_curve =
                edge->reverse == &edge->dest->reverse->edge[TRACK_EDGE_CURVED];
            bool have_curve =
                switch_iscurved(&tr->switches, edge->dest->reverse->num);
            if (want_curve != have_curve) {
                tr->reverse_ok = false;
                return;
            }
        }

        if (edge->dest->type == TRACK_NODE_EXIT)
            break;

        ix = TRACK_EDGE_AHEAD;
        if (edge->dest->type == TRACK_NODE_BRANCH) {
            if (switch_iscurved(&tr->switches, edge->dest->num))
                ix = TRACK_EDGE_CURVED;
            else
                ix = TRACK_EDGE_STRAIGHT;
        }

        edge = &edge->dest->edge[ix];
    }
    tr->reverse_ok = true;
}

static void
trainsrv_sensor_orienting(struct train *tr, track_node_t sensnode)
{
    unsigned i;
    int ahead_offs_um;
    /* Stop the train  */
    tcmux_train_speed(&tr->tcmux, tr->train_id, 0);

    /* Clear sensor state. */
    for (i = 0; i < ARRAY_SIZE(tr->sensor_mask); i++)
        tr->sensor_mask[i] = 0;

    pqueue_init(
        &tr->sensor_times,
        ARRAY_SIZE(tr->sensor_times_nodes),
        tr->sensor_times_nodes);

    /* Set up initial oriented state. */

    /* Ahead position */
    ahead_offs_um =
        tr->pctrl.reversed ? TRAIN_BACK_OFFS_UM : TRAIN_FRONT_OFFS_UM;
    tr->pctrl.ahead.edge   = &sensnode->edge[TRACK_EDGE_AHEAD];
    tr->pctrl.ahead.pos_um =
        tr->pctrl.ahead.edge->len_mm * 1000 - ahead_offs_um;

    /* Behind position */
    tr->pctrl.behind = tr->pctrl.ahead;
    track_pt_reverse(&tr->pctrl.behind);
    track_pt_advance(&tr->switches, &tr->pctrl.behind, TRAIN_LENGTH_UM);
    track_pt_reverse(&tr->pctrl.behind);
    trainsrv_determine_reverse_ok(tr);

    /* Position estimate as of what time. */
    tr->pctrl.pos_time = Time(&tr->clock);
    tr->pctrl.updated  = true;

    /* Train is now stopped. */
    tr->state       = TRAIN_RUNNING;
    tr->pctrl.state = PCTRL_STOPPED;
}

static void
trainsrv_sensor_running(struct train *tr, track_node_t sens, int time)
{
    struct train_pctrl est_pctrl;
    int ahead_offs_um;

    if (tr->pctrl.state == PCTRL_STOPPING || tr->pctrl.state == PCTRL_STOPPED)
        return;

    /* Save original for comparison */
    est_pctrl = tr->pctrl;

    ahead_offs_um =
        tr->pctrl.reversed ? TRAIN_BACK_OFFS_UM : TRAIN_FRONT_OFFS_UM;

    tr->pctrl.updated      = true;
    tr->pctrl.ahead.edge   = &sens->edge[TRACK_EDGE_AHEAD];
    tr->pctrl.ahead.pos_um =
        tr->pctrl.ahead.edge->len_mm * 1000 - ahead_offs_um;
    tr->pctrl.pos_time     = time;
    if (est_pctrl.pos_time > time)
        trainsrv_pctrl_advance_ticks(tr, est_pctrl.pos_time);

    /* Compute delta */
    tr->pctrl.lastsens = sens;
    tr->pctrl.err_um   = track_pt_distance_path(
        &tr->path,
        tr->pctrl.ahead,
        est_pctrl.ahead);

    /* Adjust behind to compensate for difference in ahead. */
    tr->pctrl.behind = tr->pctrl.ahead;
    track_pt_reverse(&tr->pctrl.behind);
    track_pt_advance(&tr->switches, &tr->pctrl.behind, TRAIN_LENGTH_UM);
    track_pt_reverse(&tr->pctrl.behind);

    /* Print log messages */
    {
        int ahead_um, ahead_cm, behind_um, behind_cm, est_ahead_um, est_ahead_cm;
        int err_um, err_cm;
        bool err_neg;
        ahead_um  = tr->pctrl.ahead.pos_um;
        ahead_cm  = ahead_um / 10000;
        ahead_um %= 10000;
        behind_um  = tr->pctrl.behind.pos_um;
        behind_cm  = behind_um / 10000;
        behind_um %= 10000;
        est_ahead_um  = est_pctrl.ahead.pos_um;
        est_ahead_cm  = est_ahead_um / 10000;
        est_ahead_um %= 10000;
        err_um  = tr->pctrl.err_um;
        err_neg = err_um < 0;
        if (err_neg)
            err_um = -err_um;
        err_cm  = err_um / 10000;
        err_um %= 10000;
        dbglog(&tr->dbglog,
            "%s: act. at %s-%d.%04dcm (behind=%s-%d.%04dcm), est. at %s-%d.%04dcm, err=%c%d.%04dcm",
            sens->name,
            tr->pctrl.ahead.edge->dest->name,
            ahead_cm,
            ahead_um,
            tr->pctrl.behind.edge->dest->name,
            behind_cm,
            behind_um,
            est_pctrl.ahead.edge->dest->name,
            est_ahead_cm,
            est_ahead_um,
            err_neg ? '-' : '+',
            err_cm,
            err_um);
    }
}

static void
trainsrv_start_orienting(struct train *tr)
{
    tcmux_train_speed(&tr->tcmux, tr->train_id, TRAIN_CRAWLSPEED);
    tr->state = TRAIN_ORIENTING;
}

static void
trainsrv_sensor_timeout(struct train *tr, int time)
{
    tr->sensor_rdy = true;
    if (tr->state == TRAIN_DISORIENTED) {
        trainsrv_start_orienting(tr);
        return;
    }

    for (;;) {
        struct pqueue_entry *first_sensor;
        int first_val;
        first_sensor = pqueue_peekmin(&tr->sensor_times);
        if (first_sensor == NULL)
            break;
        if (first_sensor->key >= time)
            break;
        first_val = first_sensor->val;
        pqueue_popmin(&tr->sensor_times);
        tr->sensor_mask[sensor1_module(first_val)] &=
            ~(1 << sensor1_sensor(first_val));
    }
}

static void
trainsrv_sensor(struct train *tr, sensors_t sensors[SENSOR_MODULES], int time)
{
    const struct track_node *sens;
    struct pqueue_entry *first_sensor;
    sensor1_t hit;

    assert(tr->state != TRAIN_DISORIENTED);

    tr->sensor_rdy = true;
    hit = sensors_scan(sensors);
    assert(hit != SENSOR1_INVALID);
    sens = &tr->track->nodes[hit];
    assert(sens->type == TRACK_NODE_SENSOR);

    if (tr->state == TRAIN_ORIENTING) {
        trainsrv_sensor_orienting(tr, sens);
        return;
    }

    for (;;) {
        int first_val;
        first_sensor = pqueue_peekmin(&tr->sensor_times);
        if (first_sensor == NULL)
            break;
        first_val = first_sensor->val;
        pqueue_popmin(&tr->sensor_times);
        tr->sensor_mask[sensor1_module(first_val)] &=
            ~(1 << sensor1_sensor(first_val));
        if (first_val == hit)
            break;
    }

    assert(tr->state == TRAIN_RUNNING);
    trainsrv_sensor_running(tr, sens, time);
}

static int
trainsrv_accel_pos(struct train *tr, int t)
{
    float x = 0, tp = 1;
    int ix;
    unsigned i;
    x = tr->calib.accel[0];
    for (i = 1; i < ARRAY_SIZE(tr->calib.accel); i++) {
        tp *= t;
        x  += tr->calib.accel[i] * tp;
    }
    ix = (int)x;
    return ix < 0 ? 0 : ix;
}

#if 0
static int
trainsrv_accel_vel(struct train *tr, int t)
{
    float v = 0, tp = 1;
    int iv;
    unsigned i;
    v = tr->calib.accel[1];
    for (i = 2; i < ARRAY_SIZE(tr->calib.accel); i++) {
        tp *= t;
        v  += i * tr->calib.accel[i] * tp;
    }
    iv = (int)v;
    return iv < 0 ? 0 : iv;
}
#endif

static int
trainsrv_predict_dist_um(struct train *tr, int when)
{
    int dist_um, dt, cutoff;
    //assert(when >= tr->pctrl.pos_time);
    switch (tr->pctrl.state) {
    case PCTRL_CRUISE:
        dist_um = (when - tr->pctrl.pos_time) * tr->calib.vel_umpt[tr->speed];
        break;
    case PCTRL_ACCEL:
        dt = when - tr->pctrl.accel_start;
        cutoff = tr->calib.accel_cutoff[tr->speed];
        if (tr->pctrl.accel_start > tr->pctrl.pos_time) {
            dt -= tr->pctrl.accel_start - tr->pctrl.pos_time;
            if (dt <= 0) {
                dist_um = 0;
                break;
            }
        }
        if (when - tr->pctrl.accel_start < cutoff) {
            dist_um  = trainsrv_accel_pos(tr, when - tr->pctrl.accel_start);
            dist_um -= trainsrv_accel_pos(tr,
                tr->pctrl.pos_time - tr->pctrl.accel_start);
        } else {
            assert(tr->pctrl.pos_time - tr->pctrl.accel_start < cutoff);
            dist_um  = trainsrv_accel_pos(tr, cutoff);
            dist_um -= trainsrv_accel_pos(tr,
                tr->pctrl.pos_time - tr->pctrl.accel_start);
            dist_um += (when - tr->pctrl.accel_start - cutoff)
                * tr->calib.vel_umpt[tr->speed];
        }
        break;
    default:
        panic("panic! bad state %d", tr->pctrl.state);
    }
    return dist_um;
}

static void
trainsrv_pctrl_advance_ticks(struct train *tr, int now)
{
    trainsrv_pctrl_advance_um(tr, trainsrv_predict_dist_um(tr, now));
    tr->pctrl.pos_time = now;
    if (tr->pctrl.state == PCTRL_ACCEL) {
        if (now - tr->pctrl.accel_start >= tr->calib.accel_cutoff[tr->speed])
            tr->pctrl.state = PCTRL_CRUISE;
    }
}

static void
trainsrv_pctrl_advance_um(struct train *tr, int dist_um)
{
    tr->pctrl.updated = true;
    track_pt_advance_path(&tr->path, &tr->pctrl.ahead,  dist_um);
    track_pt_advance_path(&tr->path, &tr->pctrl.behind, dist_um);
}

static void
trainsrv_timer(struct train *tr, int time)
{
    int dt, dist;

    if (tr->state != TRAIN_RUNNING)
        return;

    dt = time - tr->pctrl.pos_time;
    switch (tr->pctrl.state) {
    /* TODO: Estimate position while accelerating/decelerating */
    case PCTRL_STOPPED:
        break;

    case PCTRL_STOPPING:
        tr->pctrl.stop_ticks -= dt;
        if (tr->pctrl.stop_ticks < 0) {
            tr->pctrl.state = PCTRL_STOPPED;
            dist = tr->pctrl.stop_um;
            if (dist >= 0) {
                trainsrv_pctrl_advance_um(tr, dist);
                trainsrv_determine_reverse_ok(tr);
                while (pqueue_peekmin(&tr->sensor_times) != NULL)
                    pqueue_popmin(&tr->sensor_times);
            } else {
                trainsrv_start_orienting(tr); /* lost track of position */
            }
        }
        break;

    case PCTRL_ACCEL:
    case PCTRL_CRUISE:
        trainsrv_pctrl_advance_ticks(tr, time);
        break;

    default:
        panic("bad train position control state %d", tr->pctrl.state);
    }
}

static void
trainsrv_expect_sensor(struct train *tr, track_node_t sens)
{
    struct track_pt sens_pt;
    int smod, ssens;
    int rc, ticks, sens_dist_um, vel;

    assert(sens->type == TRACK_NODE_SENSOR);

    smod  = sensor1_module(sens->num);
    ssens = 1 << sensor1_sensor(sens->num);
    if (tr->sensor_mask[smod] & ssens)
        return;

    /* Calculate expected sensor time */
    sens_pt.edge   = sens->reverse->edge[TRACK_EDGE_AHEAD].reverse;
    sens_pt.pos_um = 0;
    sens_dist_um   = track_pt_distance_path(&tr->path, tr->pctrl.ahead, sens_pt);
    sens_dist_um  += tr->pctrl.reversed ? TRAIN_BACK_OFFS_UM : TRAIN_FRONT_OFFS_UM;
    sens_dist_um  += TRAIN_SENSOR_OVERESTIMATE_UM;
    vel            = tr->calib.vel_umpt[tr->speed];
    ticks          = tr->pctrl.pos_time + sens_dist_um / vel;

    tr->sensor_mask[smod] |= ssens;
    rc = pqueue_add(&tr->sensor_times, sens->num, ticks);
    assertv(rc, rc == 0);
}

static void
trainsrv_tryrequest_sensor(struct train *tr)
{
    struct sensor_reply reply;
    struct pqueue_entry *min;
    int rc;

    if (!tr->sensor_rdy || tr->state == TRAIN_DISORIENTED)
        return;

    if (tr->state == TRAIN_ORIENTING) {
        min = NULL;
    } else {
        /* Find earliest time, removing those in the past. */
        for (;;) {
            int min_val;
            min = pqueue_peekmin(&tr->sensor_times);
            if (min == NULL)
                return;
            if (min->key >= tr->pctrl.pos_time
                || tr->pctrl.state != PCTRL_CRUISE)
                break;
            min_val = min->val;
            pqueue_popmin(&tr->sensor_times);
            tr->sensor_mask[sensor1_module(min_val)] &=
                ~(1 << sensor1_sensor(min_val));
        }
    }

    if (tr->state != TRAIN_RUNNING) {
        reply.timeout = -1;
    } else if (tr->pctrl.state == PCTRL_STOPPING) {
        reply.timeout = tr->pctrl.stop_ticks;
    } else if (tr->pctrl.state != PCTRL_CRUISE) {
        reply.timeout = -1;
    } else {
        reply.timeout = min->key - tr->pctrl.pos_time;
        assert(reply.timeout >= 0);
    }

    if (tr->state == TRAIN_ORIENTING)
        memset(reply.sensors, 0xff, sizeof (reply.sensors));
    else
        memcpy(reply.sensors, tr->sensor_mask, sizeof (reply.sensors));

    dbglog(&tr->dbglog, "wait for %04x-%04x-%04x-%04x-%04x, timeout=%d",
        reply.sensors[0],
        reply.sensors[1],
        reply.sensors[2],
        reply.sensors[3],
        reply.sensors[4],
        reply.timeout);

    rc = Reply(tr->sensor_tid, &reply, sizeof (reply));
    assertv(rc, rc == 0);
    tr->sensor_rdy = false;
}

static void
trainsrv_pctrl_check_update(struct train *tr)
{
    unsigned i, nsensors;

    if (!tr->pctrl.updated)
        return;

    /* Reply to estimate client if any */
    trainsrv_send_estimate(tr);

    /* Reset updated flag. */
    tr->pctrl.updated = false;

    /* Position control follows. Only while moving! */
    if (tr->state != TRAIN_RUNNING ||
        (tr->pctrl.state != PCTRL_CRUISE && tr->pctrl.state != PCTRL_ACCEL))
        return;

    /* Calculate current stopping point and stop if desired. */
    if (tr->pctrl.state == PCTRL_CRUISE || tr->pctrl.state == PCTRL_ACCEL) {
        int distance_um;
        distance_um = track_pt_distance_path(
            &tr->path,
            tr->pctrl.ahead,
            tr->path.end);

        if (distance_um <= tr->calib.stop_um[tr->speed]) {
            trainsrv_stop(tr);
            return;
        }
    }

    /* Check for branches ahead. */
    trainsrv_pctrl_switch_turnouts(tr, false);

    /* Check for sensors ahead. */
    i = TRACK_NODE_DATA(tr->track, tr->pctrl.ahead.edge->src, tr->path.node_ix);
    nsensors = 0;
    while (i < tr->path.hops && nsensors < 2) {
        track_node_t node = tr->path.edges[i++]->dest;
        if (node->type != TRACK_NODE_SENSOR)
            continue;
        trainsrv_expect_sensor(tr, node);
        nsensors++;
    }
}

static void
trainsrv_pctrl_switch_turnouts(struct train *tr, bool switch_all)
{
    while ((unsigned)tr->path_swnext < tr->path.hops) {
        struct track_pt sw_pt;
        track_node_t branch;
        bool curved;

        branch = tr->path.edges[tr->path_swnext]->src;
        if (branch->type == TRACK_NODE_BRANCH) {
            sw_pt.edge   = tr->path.edges[tr->path_swnext];
            sw_pt.pos_um = 1000 * sw_pt.edge->len_mm;
        } else {
            assert(branch->type == TRACK_NODE_MERGE);
            sw_pt.edge   = tr->path.edges[tr->path_swnext - 1];
            sw_pt.pos_um = TRAIN_MERGE_OFFSET_UM;
        }

        if (!switch_all) {
            int dist_um;
            dist_um  = track_pt_distance_path(&tr->path, tr->pctrl.ahead, sw_pt);
            dist_um -= trainsrv_predict_dist_um(tr,
                tr->pctrl.pos_time + TRAIN_SWITCH_AHEAD_TICKS);
            if (dist_um > 0)
                break;
        }

        if (branch->type == TRACK_NODE_BRANCH) {
            curved = sw_pt.edge == &sw_pt.edge->src->edge[TRACK_EDGE_CURVED];
        } else {
            curved = sw_pt.edge
                == sw_pt.edge->reverse->src->edge[TRACK_EDGE_CURVED].reverse;
        }

        dbglog(&tr->dbglog, "setting %d to %s", branch->num, curved ? "curved" : "straight");
        tcmux_switch_curve(&tr->tcmux, branch->num, curved);
        trainsrv_swnext_advance(tr);
    }
}

static void
trainsrv_send_estimate(struct train *tr)
{
    struct trainest est;
    int rc;

    assert(tr->pctrl.updated);
    if (tr->estimate_client < 0)
        return;

    est.train_id = tr->train_id;
    est.ahead    = tr->pctrl.ahead;
    est.behind   = tr->pctrl.behind;
    est.lastsens = tr->pctrl.lastsens;

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
        if (rc == SENSOR_TIMEOUT) {
            memset(msg.sensors, '\0', sizeof (msg.sensors));
            msg.type = TRAINMSG_SENSOR_TIMEOUT;
        } else {
            memcpy(msg.sensors, reply.sensors, sizeof (msg.sensors));
            msg.type = TRAINMSG_SENSOR;
        }
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
    int rc;

    train = MyParentTid();
    clkctx_init(&clock);

    msg.type = TRAINMSG_TIMER;
    msg.time = Time(&clock);
    for (;;) {
        int rplylen;
        msg.time += TRAIN_TIMER_TICKS;
        rc = DelayUntil(&clock, msg.time);
        (void)rc; // assertv(rc, rc == CLOCK_OK);
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
trainctx_init(struct trainctx *ctx, uint8_t train_id)
{
    char buf[32];
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
train_moveto(struct trainctx *ctx, struct track_pt dest)
{
    struct trainmsg msg;
    int rplylen;
    msg.type = TRAINMSG_MOVETO;
    msg.dest = dest;
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
train_estimate(struct trainctx *ctx, struct trainest *est)
{
    struct trainmsg msg;
    int rplylen;
    msg.type = TRAINMSG_ESTIMATE;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), est, sizeof (*est));
    assertv(rplylen, rplylen == sizeof (*est));
}

static void
trainsrv_fmt_name(uint8_t train_id, char buf[32])
{
    struct ringbuf rbuf;
    rbuf_init(&rbuf, buf, 32);
    rbuf_printf(&rbuf, "train%d", train_id);
    rbuf_putc(&rbuf, '\0');
}
