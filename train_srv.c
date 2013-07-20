#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "poly.h"
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

#define TRAIN_ACCEL_STABILIZE_TICKS     50
#define TRAIN_SENSOR_OVERESTIMATE_UM    (80 * 1000) // FIXME more like 4cm
#define TRAIN_SWITCH_AHEAD_TICKS        100
#define TRAIN_SENSOR_AHEAD_TICKS        100
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
    int                       est_time, accel_start;
    int                       vel_umpt;
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
    struct sensorctx          sensors;

    /* General train state */
    const struct track_graph *track;
    uint8_t                   train_id;
    uint8_t                   speed;
    uint8_t                   desired_speed;

    /* Calibration data */
    struct calib              calib;

    /* Timer */
    tid_t                     timer_tid;

    /* Position estimate */
    struct train_pctrl        pctrl;
    tid_t                     estimate_client;

    /* Path to follow */
    struct track_route        route;
    struct track_path        *path;
    int                       path_swnext;
    int                       path_sensnext;
    bool                      reverse_ok;
};

static void trainsrv_init(struct train *tr, struct traincfg *cfg);
static void trainsrv_orient_train(struct train *t);
static void trainsrv_setspeed(struct train *tr, uint8_t speed);
static void trainsrv_swnext_init(struct train *tr);
static void trainsrv_swnext_advance(struct train *tr);
static void trainsrv_moveto(struct train *tr, struct track_pt dest);
static void trainsrv_stop(struct train *tr);
static void trainsrv_sensor_running(struct train *tr, track_node_t sens, int time);
static void trainsrv_sensor(
    struct train *tr, sensors_t sensors[SENSOR_MODULES], int time);
static void trainsrv_sensor_timeout(struct train *tr, int time);
static int  trainsrv_accel_pos(struct train *tr, int t);
static int  trainsrv_predict_dist_um(struct train *tr, int when);
static int trainsrv_predict_vel_umpt(struct train *tr, int when);
static void trainsrv_pctrl_advance_ticks(struct train *tr, int now);
static void trainsrv_pctrl_advance_um(struct train *tr, int dist_um);
static void trainsrv_timer(struct train *tr, int time);
static void trainsrv_expect_sensor(struct train *tr, track_node_t sens);
static void trainsrv_pctrl_check_update(struct train *tr);
static void trainsrv_pctrl_expect_sensors(struct train *tr);
static void trainsrv_pctrl_switch_turnouts(struct train *tr, bool all);
static void trainsrv_send_estimate(struct train *tr, tid_t client);

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

    trainsrv_fmt_name(cfg.train_id, srv_name);
    rc = RegisterAs(srv_name);
    assertv(rc, rc == 0);

    trainsrv_init(&tr, &cfg);

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
            trainsrv_empty_reply(client);
            trainsrv_sensor(&tr, msg.sensors, msg.time);
            break;
        case TRAINMSG_SENSOR_TIMEOUT:
            trainsrv_empty_reply(client);
            trainsrv_sensor_timeout(&tr, msg.time);
            break;
        case TRAINMSG_TIMER:
            assert(client == tr.timer_tid);
            trainsrv_empty_reply(client);
            trainsrv_timer(&tr, msg.time);
            break;
        case TRAINMSG_ESTIMATE:
            trainsrv_send_estimate(&tr, client);
            break;
        default:
            panic("invalid train message type %d", msg.type);
        }
        trainsrv_pctrl_check_update(&tr);
    }
}

static void
trainsrv_init(struct train *tr, struct traincfg *cfg)
{
    tr->track         = tracksel_ask();
    tr->train_id      = cfg->train_id;
    tr->desired_speed = TRAIN_DEFSPEED;

    tr->pctrl.state = PCTRL_STOPPED;
    tr->pctrl.est_time = -1;
    tr->pctrl.reversed = false;
    tr->pctrl.lastsens = NULL;
    tr->pctrl.updated  = false;
    tr->pctrl.vel_umpt = 0;
    tr->estimate_client = -1;

    /* take initial calibration */
    calib_get(tr->train_id, &tr->calib);

    tr->timer_tid = Create(PRIORITY_TRAIN - 1, &trainsrv_timer_listen);
    assert(tr->timer_tid >= 0);

    clkctx_init(&tr->clock);
    tcmuxctx_init(&tr->tcmux);
    switchctx_init(&tr->switches);
    dbglogctx_init(&tr->dbglog);
    sensorctx_init(&tr->sensors);

    trainsrv_orient_train(tr);
}

static void
trainsrv_setspeed(struct train *tr, uint8_t speed)
{
    assert(speed > 0 && speed < 15);
    if (speed < TRAIN_MINSPEED)
        speed = TRAIN_MINSPEED;
    if (speed > TRAIN_MAXSPEED)
        speed = TRAIN_MAXSPEED;
    tr->desired_speed = speed;
    /* TODO? maybe change while we're moving? MAYBE */
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
    if (tr->path_swnext >= 0 && (unsigned)tr->path_swnext >= tr->path->hops)
        return;
    tr->path_swnext++;
    if (tr->path_swnext == 0
        && tr->path->edges[tr->path_swnext]->src->type == TRACK_NODE_MERGE)
        tr->path_swnext++;
    while ((unsigned)tr->path_swnext < tr->path->hops) {
        int type = tr->path->edges[tr->path_swnext]->src->type;
        if (type == TRACK_NODE_MERGE || type == TRACK_NODE_BRANCH)
            break;
        tr->path_swnext++;
    }
}

static void
trainsrv_moveto(struct train *tr, struct track_pt dest)
{
    struct track_routespec rspec;
    int rc;
    if (tr->pctrl.state != PCTRL_STOPPED)
        return;

    /* Get path to follow */
    track_routespec_init(&rspec);
    rspec.track        = tr->track;
    rspec.switches     = tr->switches;
    rspec.src_centre   = tr->pctrl.ahead;
    rspec.err_um       = 0; /* FIXME */
    rspec.init_rev_ok  = tr->reverse_ok;
    rspec.rev_ok       = true;
    rspec.train_len_um = TRAIN_LENGTH_UM; /* FIXME */
    rspec.dest         = dest;

    /* Adjust centre FIXME this should go away. */
    track_pt_reverse(&rspec.src_centre);
    track_pt_advance(&tr->switches, &rspec.src_centre, TRAIN_LENGTH_UM / 2);
    track_pt_reverse(&rspec.src_centre);

    rc = track_routefind(&rspec, &tr->route);
    if (rc < 0 || tr->route.n_paths == 0)
        return;

    tr->path = &tr->route.paths[0];
    if (tr->path->hops == 0)
        return;

    /* Initial reverse if necessary */
    if (tr->path->edges[0] == rspec.src_centre.edge->reverse) {
        struct track_pt tmp_pt;
        tcmux_train_speed(&tr->tcmux, tr->train_id, 15);
        tmp_pt           = tr->pctrl.ahead;
        tr->pctrl.ahead  = tr->pctrl.behind;
        tr->pctrl.behind = tmp_pt;
        track_pt_reverse(&tr->pctrl.ahead);
        track_pt_reverse(&tr->pctrl.behind);
        tr->pctrl.reversed = !tr->pctrl.reversed;
    }

    /* Find speed to use. */
    for (tr->speed = tr->desired_speed; tr->speed >= TRAIN_MINSPEED; tr->speed--) {
        int accel_time     = tr->calib.accel_cutoff[tr->speed];
        int accel_end_um   = polyeval(&tr->calib.accel, accel_time);
        int vel_umpt       = tr->calib.vel_umpt[tr->speed];
        int stop_um        = polyeval(&tr->calib.stop_um, vel_umpt);
        int min_allowed_um = accel_end_um + stop_um;
        min_allowed_um    += vel_umpt * TRAIN_ACCEL_STABILIZE_TICKS;
        dbglog(&tr->dbglog,
            "considering speed %d: min allowed %d um vs. needed %d um",
            tr->speed,
            min_allowed_um,
            1000 * (int)tr->path->len_mm);
        if ((unsigned)min_allowed_um <= 1000 * tr->path->len_mm)
            break;
    }
    /* FIXME clamping the speed to minimum */
    if (tr->speed < TRAIN_MINSPEED)
        tr->speed = TRAIN_MINSPEED;

    /* Set up motion state */
    trainsrv_swnext_init(tr);
    tr->path_sensnext = 1; /* Ignore first sensor if it is the source
                            * of the first edge on the path */
    tr->pctrl.vel_umpt = 0;
    tr->pctrl.est_time = Time(&tr->clock);
    tr->pctrl.updated  = true;

    tcmux_train_speed(&tr->tcmux, tr->train_id, tr->speed);
    dbglog(&tr->dbglog, "train%d sending speed %d (v=%d um/tick)",
        tr->train_id,
        tr->speed,
        tr->calib.vel_umpt[tr->speed]);
    /* already TRAIN_RUNNING */
    tr->pctrl.state = PCTRL_ACCEL;
    tr->pctrl.accel_start = tr->pctrl.est_time + tr->calib.accel_delay;
}

static void
trainsrv_stop(struct train *tr)
{
    if (tr->pctrl.state == PCTRL_STOPPING || tr->pctrl.state == PCTRL_STOPPED)
        return; /* ignore */

    tcmux_train_speed(&tr->tcmux, tr->train_id, 0);

    tr->pctrl.state      = PCTRL_STOPPING;
    tr->pctrl.stop_ticks = PCTRL_STOP_TICKS;
    tr->pctrl.stop_um    = polyeval(&tr->calib.stop_um, tr->pctrl.vel_umpt);

    dbglog(&tr->dbglog, "train%d stopping at %s-%dum, v=%dum/cs, d=%dum",
        tr->train_id,
        tr->pctrl.ahead.edge->dest->name,
        tr->pctrl.ahead.pos_um,
        tr->pctrl.vel_umpt,
        tr->pctrl.stop_um);

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
trainsrv_orient_train(struct train *tr)
{
    int ahead_offs_um, i, rc;
    sensors_t all_sensors[SENSOR_MODULES];
    track_node_t sensnode;

    /* Start the train crawling */
    tcmux_train_speed(&tr->tcmux, tr->train_id, TRAIN_CRAWLSPEED);

    /* Wait for all sensors */
    for (i = 0; i < SENSOR_MODULES; i++)
        all_sensors[i] = (sensors_t)-1;

    rc = sensor_wait(&tr->sensors, all_sensors, -1, NULL);
    assertv(rc, rc == SENSOR_TRIPPED);

    /* Stop the train  */
    tcmux_train_speed(&tr->tcmux, tr->train_id, 0);

    /* Determine which sensor was hit */
    sensnode = &tr->track->nodes[sensors_scan(all_sensors)];

    /* Set up initial oriented state. */

    /* Ahead position */
    ahead_offs_um =
        tr->pctrl.reversed ? TRAIN_BACK_OFFS_UM : TRAIN_FRONT_OFFS_UM;
    tr->pctrl.ahead.edge   = &sensnode->edge[TRACK_EDGE_AHEAD];
    tr->pctrl.ahead.pos_um =
        tr->pctrl.ahead.edge->len_mm * 1000 - ahead_offs_um;

    /* Adjust ahead position for stopping distance of crawling speed. */
    track_pt_advance(
        &tr->switches,
        &tr->pctrl.ahead,
        polyeval(&tr->calib.stop_um, tr->calib.vel_umpt[TRAIN_CRAWLSPEED]));

    /* Behind position */
    tr->pctrl.behind = tr->pctrl.ahead;
    track_pt_reverse(&tr->pctrl.behind);
    track_pt_advance(&tr->switches, &tr->pctrl.behind, TRAIN_LENGTH_UM);
    track_pt_reverse(&tr->pctrl.behind);

    /* Initial velocity is zero */
    tr->speed          = 0;
    tr->pctrl.vel_umpt = 0;

    /* Determine whether it's okay to reverse from here */
    trainsrv_determine_reverse_ok(tr);

    /* Position estimate as of what time. */
    tr->pctrl.est_time = Time(&tr->clock);
    tr->pctrl.updated  = true;

    /* Train is now stopped. */
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
    tr->pctrl.est_time     = time;
    if (est_pctrl.est_time > time)
        trainsrv_pctrl_advance_ticks(tr, est_pctrl.est_time);

    /* Compute delta */
    tr->pctrl.lastsens = sens;
    tr->pctrl.err_um   = track_pt_distance_path(
        tr->path,
        tr->pctrl.ahead,
        est_pctrl.ahead);

    if (tr->pctrl.err_um > 0) {
        /* Might have expected later sensors with too early a timeout. */
        tr->path_sensnext = TRACK_NODE_DATA(tr->track, sens, tr->path->node_ix);
        tr->path_sensnext++;
    }

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
trainsrv_sensor_timeout(struct train *tr, int time)
{
    (void)tr;
    (void)time;
}

static void
trainsrv_sensor(struct train *tr, sensors_t sensors[SENSOR_MODULES], int time)
{
    const struct track_node *sens;
    sensor1_t hit;

    hit = sensors_scan(sensors);
    assert(hit != SENSOR1_INVALID);
    sens = &tr->track->nodes[hit];
    assert(sens->type == TRACK_NODE_SENSOR);

    trainsrv_sensor_running(tr, sens, time);
}

static int
trainsrv_accel_pos(struct train *tr, int t)
{
    if (t <= 0)
        return 0;
    return (int)polyeval(&tr->calib.accel, (float)t);
}

static int
trainsrv_predict_dist_um(struct train *tr, int when)
{
    int dist_um, dt, cutoff;
    //assert(when >= tr->pctrl.est_time);
    switch (tr->pctrl.state) {
    case PCTRL_CRUISE:
        dist_um = (when - tr->pctrl.est_time) * tr->calib.vel_umpt[tr->speed];
        break;
    case PCTRL_ACCEL:
        dt = when - tr->pctrl.accel_start;
        cutoff = tr->calib.accel_cutoff[tr->speed];
        if (tr->pctrl.accel_start > tr->pctrl.est_time) {
            dt -= tr->pctrl.accel_start - tr->pctrl.est_time;
        }
        if (dt <= 0) {
            dist_um = 0;
            break;
        }
        if (dt < cutoff) {
            dist_um  = trainsrv_accel_pos(tr, dt);
            dist_um -= trainsrv_accel_pos(tr,
                tr->pctrl.est_time - tr->pctrl.accel_start);
        } else {
            assert(tr->pctrl.est_time - tr->pctrl.accel_start < cutoff);
            dist_um  = trainsrv_accel_pos(tr, cutoff);
            dist_um -= trainsrv_accel_pos(tr,
                tr->pctrl.est_time - tr->pctrl.accel_start);
            dist_um += (dt - cutoff) * tr->calib.vel_umpt[tr->speed];
        }
        break;
    default:
        panic("panic! bad state %d", tr->pctrl.state);
    }
    return dist_um;
}

static int
trainsrv_predict_vel_umpt(struct train *tr, int when)
{
    int vel_umpt, dt, cutoff;
    //assert(when >= tr->pctrl.est_time);
    switch (tr->pctrl.state) {
    case PCTRL_CRUISE:
        vel_umpt = tr->calib.vel_umpt[tr->speed];
        break;
    case PCTRL_ACCEL:
        dt = when - tr->pctrl.accel_start;
        cutoff = tr->calib.accel_cutoff[tr->speed];
        if (tr->pctrl.accel_start > tr->pctrl.est_time) {
            dt -= tr->pctrl.accel_start - tr->pctrl.est_time;
        }
        if (dt <= 0)
            vel_umpt = tr->pctrl.vel_umpt;
        else if (dt >= cutoff)
            vel_umpt = tr->calib.vel_umpt[tr->speed];
        else {
            struct poly v;
            polydiff(&tr->calib.accel, &v);
            vel_umpt = (int)polyeval(&v, dt);
        }
        break;
    default:
        panic("panic! bad state %d", tr->pctrl.state);
    }
    return vel_umpt;
}

static void
trainsrv_pctrl_advance_ticks(struct train *tr, int now)
{
    int dt, cutoff;
    /* Update position. */
    trainsrv_pctrl_advance_um(tr, trainsrv_predict_dist_um(tr, now));
    /* Update velocity. */
    tr->pctrl.vel_umpt = trainsrv_predict_vel_umpt(tr, now);
    /* Update state */
    dt     = now - tr->pctrl.accel_start;
    cutoff = tr->calib.accel_cutoff[tr->speed];
    if (tr->pctrl.state == PCTRL_ACCEL && dt >= cutoff)
        tr->pctrl.state = PCTRL_CRUISE;
    /* Update time of estimate. */
    tr->pctrl.est_time = now;
}

static void
trainsrv_pctrl_advance_um(struct train *tr, int dist_um)
{
    tr->pctrl.updated = true;
    track_pt_advance_path(tr->path, &tr->pctrl.ahead,  dist_um);
    track_pt_advance_path(tr->path, &tr->pctrl.behind, dist_um);
}

static void
trainsrv_timer(struct train *tr, int time)
{
    int dt, dist;

    dt = time - tr->pctrl.est_time;
    switch (tr->pctrl.state) {
    /* TODO: Estimate position while accelerating/decelerating */
    case PCTRL_STOPPED:
        break;

    case PCTRL_STOPPING:
        tr->pctrl.stop_ticks -= dt;
        if (tr->pctrl.stop_ticks < 0) {
            tr->pctrl.state = PCTRL_STOPPED;
            dist = tr->pctrl.stop_um;
            assert(dist >= 0);
            trainsrv_pctrl_advance_um(tr, dist);
            trainsrv_determine_reverse_ok(tr);
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

struct sens_param {
    struct sensorctx ctx;
    track_node_t sens;
};

static void
sensor_worker(void)
{
    int rc;
    tid_t train;
    int time;
    sensors_t sensors[SENSOR_MODULES] = { 0 };
    struct trainmsg msg;
    struct sens_param sp;

    rc = Receive(&train, &sp, sizeof(sp));
    assertv(rc, rc == sizeof(sp));
    rc = Reply(train, NULL, 0);
    assertv(rc, rc == 0);

    sensors[sensor1_module(sp.sens->num)] = (1 << sensor1_sensor(sp.sens->num));

    rc = sensor_wait(&sp.ctx, sensors, 2 * TRAIN_SENSOR_AHEAD_TICKS, &time);
    msg.time = time;
    if (rc == SENSOR_TRIPPED) {
        msg.type = TRAINMSG_SENSOR;
        memcpy(msg.sensors, sensors, sizeof(sensors));
    } else {
        msg.type = TRAINMSG_SENSOR_TIMEOUT;
    }

    rc = Send(train, &msg, sizeof(msg), NULL, 0);
    assertv(rc, rc == 0);
}

static void
trainsrv_expect_sensor(struct train *tr, track_node_t sens)
{
    int replylen;
    tid_t worker;
    struct sens_param sp;

    sp.ctx = tr->sensors;
    sp.sens = sens;

    worker = Create(PRIORITY_TRAIN - 1, &sensor_worker);
    assert(worker >= 0);

    replylen = Send(worker, &sp, sizeof(sp), NULL, 0);
    assertv(replylen, replylen == 0);
}

static void
trainsrv_pctrl_check_update(struct train *tr)
{
    if (!tr->pctrl.updated)
        return;

    /* Reset updated flag. */
    tr->pctrl.updated = false;

    /* Position control follows. Only while moving! */
    if (tr->pctrl.state != PCTRL_CRUISE && tr->pctrl.state != PCTRL_ACCEL)
        return;

    /* Calculate current stopping point and stop if desired. */
    if (tr->pctrl.state == PCTRL_CRUISE || tr->pctrl.state == PCTRL_ACCEL) {
        int stop_um, end_um;
        stop_um = polyeval(&tr->calib.stop_um, tr->pctrl.vel_umpt);
        end_um = track_pt_distance_path(
            tr->path,
            tr->pctrl.ahead,
            tr->path->end);
        if (end_um <= stop_um) {
            trainsrv_stop(tr);
            return;
        }
    }

    /* Check for branches ahead. */
    trainsrv_pctrl_switch_turnouts(tr, false);

    /* Check for sensors ahead. */
    trainsrv_pctrl_expect_sensors(tr);
}

static void
trainsrv_pctrl_expect_sensors(struct train *tr)
{
    for(;(unsigned)tr->path_sensnext < tr->path->hops; tr->path_sensnext++) {
        struct track_pt sens_pt;
        track_node_t sens;
        int sensdist_um, travel_um;

        sens = tr->path->edges[tr->path_sensnext]->src;
        if (sens->type != TRACK_NODE_SENSOR)
            continue;

        track_pt_from_node(sens, &sens_pt);

        sensdist_um = track_pt_distance_path(tr->path, tr->pctrl.ahead, sens_pt);
        if (tr->pctrl.reversed)
            sensdist_um += TRAIN_BACK_OFFS_UM;
        else
            sensdist_um += TRAIN_FRONT_OFFS_UM;

        travel_um = trainsrv_predict_dist_um(
            tr, 
            tr->pctrl.est_time + TRAIN_SENSOR_AHEAD_TICKS);

        if (sensdist_um > travel_um) {
            break;
        }

        trainsrv_expect_sensor(tr, sens);
    }
}

static void
trainsrv_pctrl_switch_turnouts(struct train *tr, bool switch_all)
{
    while ((unsigned)tr->path_swnext < tr->path->hops) {
        struct track_pt sw_pt;
        track_node_t branch;
        bool curved;

        branch = tr->path->edges[tr->path_swnext]->src;
        if (branch->type == TRACK_NODE_BRANCH) {
            sw_pt.edge   = tr->path->edges[tr->path_swnext];
            sw_pt.pos_um = 1000 * sw_pt.edge->len_mm;
        } else {
            assert(branch->type == TRACK_NODE_MERGE);
            sw_pt.edge   = tr->path->edges[tr->path_swnext - 1];
            sw_pt.pos_um = TRAIN_MERGE_OFFSET_UM;
        }

        if (!switch_all) {
            int dist_um;
            dist_um  = track_pt_distance_path(tr->path, tr->pctrl.ahead, sw_pt);
            dist_um -= trainsrv_predict_dist_um(tr,
                tr->pctrl.est_time + TRAIN_SWITCH_AHEAD_TICKS);
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
trainsrv_send_estimate(struct train *tr, tid_t client)
{
    struct trainest est;
    int rc;

    est.train_id = tr->train_id;
    est.ahead    = tr->pctrl.ahead;
    est.behind   = tr->pctrl.behind;
    est.lastsens = tr->pctrl.lastsens;
    est.err_mm   = tr->pctrl.err_um / 1000;

    rc = Reply(client, &est, sizeof (est));
    assertv(rc, rc == 0);
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
