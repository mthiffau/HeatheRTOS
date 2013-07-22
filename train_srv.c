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
#include "track_srv.h"
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
#define TRAIN_ORIENT_ERR_UM             50000  // 5cm
#define TRAIN_RES_NEED_TICKS            10
#define TRAIN_RES_WANT_TICKS            20
#define TRAIN_RES_GRACE_TICKS           20

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
    struct track_pt           centre;
    int                       est_time, accel_start;
    int                       vel_umpt;
    int                       stop_ticks, stop_um;
    bool                      reversed;
    const struct track_node  *lastsens;
    int                       err_um;
    bool                      updated;
};

struct respath {
    track_edge_t edges[32];
    int          earliest, next, count;
};

struct train {
    /* Server handles */
    struct clkctx             clock;
    struct tcmuxctx           tcmux;
    struct switchctx          switches;
    struct trackctx           res;
    struct sensorctx          sensors;
    struct dbglogctx          dbglog;

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

    /* Reservation info */
    struct respath            respath;
    int                       res_ahead_um, res_behind_um;
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
    trackctx_init(&tr->res, tr->train_id);
    sensorctx_init(&tr->sensors);
    dbglogctx_init(&tr->dbglog);

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
trainsrv_embark(struct train *tr)
{
    /* Find speed to use. */
    for (tr->speed = tr->desired_speed; tr->speed >= TRAIN_MINSPEED; tr->speed--) {
        int accel_time     = tr->calib.accel_cutoff[tr->speed];
        int accel_end_um   = polyeval(&tr->calib.accel, accel_time);
        int vel_umpt       = tr->calib.vel_umpt[tr->speed];
        int stop_um        = polyeval(&tr->calib.stop_um, vel_umpt);
        int min_allowed_um = accel_end_um + stop_um;
        min_allowed_um    += vel_umpt * TRAIN_ACCEL_STABILIZE_TICKS;
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
    /* already TRAIN_RUNNING */
    tr->pctrl.state = PCTRL_ACCEL;
    tr->pctrl.accel_start = tr->pctrl.est_time + tr->calib.accel_delay;

    dbglog(&tr->dbglog, "train%d sending speed %d (v=%d um/tick)",
        tr->train_id,
        tr->speed,
        tr->calib.vel_umpt[tr->speed]);
    dbglog(&tr->dbglog, "train%d moving from %s-%dmm to %s-%dmm",
        tr->train_id,
        tr->path->start.edge->dest->name,
        tr->path->start.pos_um / 1000,
        tr->path->end.edge->dest->name,
        tr->path->end.pos_um / 1000);
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
    rspec.switches     = &tr->switches;
    rspec.src_centre   = tr->pctrl.centre;
    rspec.err_um       = 50000; /* FIXME hardcoded 5cm */
    rspec.init_rev_ok  = false; /* tr->reverse_ok; FIXME reverse is broken */
    rspec.rev_ok       = false; /* true; FIXME reverse is broken */
    rspec.train_len_um = TRAIN_LENGTH_UM; /* FIXME */
    rspec.dest         = dest;

    rc = track_routefind(&rspec, &tr->route);
    if (rc < 0 || tr->route.n_paths == 0)
        return;

    tr->path = &tr->route.paths[0];
    if (tr->path->hops == 0)
        return;

#if 0
    {
        struct ringbuf rbuf;
        int i, j;
        char mem[8192];
        rbuf_init(&rbuf, mem, sizeof (mem));
        rbuf_printf(&rbuf, "\e[2J");
        for (i = 0; i < tr->route.n_paths; i++) {
            struct track_path *path = &tr->route.paths[i];
            rbuf_printf(&rbuf, "path %d:", i);
            rbuf_printf(&rbuf, " [start=%s-%dmm, end=%s-%dmm, hops=%d]",
                path->start.edge->dest->name,
                path->start.pos_um / 1000,
                path->end.edge->dest->name,
                path->end.pos_um / 1000,
                (int)path->hops);
            for (j = 0; j < (int)path->hops; j++)
                rbuf_printf(&rbuf, " [%s->%s]",
                    path->edges[j]->src->name,
                    path->edges[j]->dest->name);
            rbuf_putc(&rbuf, '\n');
        }
        rbuf_putc(&rbuf, '\0');
        panic("%s", mem);
    }
#endif

    /* Initial reverse if necessary */
    if (tr->path->edges[0] == rspec.src_centre.edge->reverse) {
        tcmux_train_speed(&tr->tcmux, tr->train_id, 15);
        track_pt_reverse(&tr->pctrl.centre);
        tr->pctrl.reversed = !tr->pctrl.reversed;
    }

    /* Go! */
    trainsrv_embark(tr);
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
        tr->pctrl.centre.edge->dest->name,
        tr->pctrl.centre.pos_um,
        tr->pctrl.vel_umpt,
        tr->pctrl.stop_um);

    /* Switch all remaining switches on path, since estimation stops.
     * FIXME this goes away with better estimations */
    trainsrv_pctrl_switch_turnouts(tr, true);
}

static void
trainsrv_orient_train(struct train *tr)
{
    int ahead_offs_um, i, rc;
    sensors_t all_sensors[SENSOR_MODULES];
    track_node_t sensnode;
    struct track_pt under_pt;

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

    /* Centre position. Calculated by going from the front of the train. */
    ahead_offs_um = TRAIN_FRONT_OFFS_UM;
    tr->pctrl.centre.edge   = &sensnode->edge[TRACK_EDGE_AHEAD];
    tr->pctrl.centre.pos_um =
        tr->pctrl.centre.edge->len_mm * 1000 - ahead_offs_um;

    track_pt_reverse(&tr->pctrl.centre);
    track_pt_advance(&tr->switches, &tr->pctrl.centre, TRAIN_LENGTH_UM / 2);
    track_pt_reverse(&tr->pctrl.centre);

    /* Adjust centre position for stopping distance of crawling speed. */
    track_pt_advance(
        &tr->switches,
        &tr->pctrl.centre,
        polyeval(&tr->calib.stop_um, tr->calib.vel_umpt[TRAIN_CRAWLSPEED]));

    /* Initial velocity is zero */
    tr->speed          = 0;
    tr->pctrl.vel_umpt = 0;
    tr->reverse_ok     = false;

    /* Position estimate as of what time. */
    tr->pctrl.est_time = Time(&tr->clock);
    tr->pctrl.updated  = true;

    /* Train is now stopped. */
    tr->pctrl.state = PCTRL_STOPPED;

    /* Reserve current position. */
    under_pt = tr->pctrl.centre;
    track_pt_reverse(&under_pt);
    track_pt_advance(
        &tr->switches,
        &under_pt,
        TRAIN_LENGTH_UM / 2 + TRAIN_ORIENT_ERR_UM);
    track_pt_reverse(&under_pt);

    track_pt_advance_trace(
        &tr->switches,
        &under_pt,
        TRAIN_LENGTH_UM + 2 * TRAIN_ORIENT_ERR_UM,
        tr->respath.edges,
        &tr->respath.count,
        ARRAY_SIZE(tr->respath.edges));

    tr->respath.earliest = 0;
    tr->respath.next     = tr->respath.count;
    for (i = 0; i < tr->respath.count; i++) {
        bool success = track_reserve(&tr->res, tr->respath.edges[i]);
        if (success)
            continue;

        dbglog(&tr->dbglog,
            "train%d failed to reserve initial position",
            tr->train_id);
        while (--i >= 0)
            track_release(&tr->res, tr->respath.edges[i]);
        Exit();
    }

    /* Initialize reservation distances */
    tr->res_behind_um = 0;
    tr->res_ahead_um  = 0;
    for (i = 0; i < tr->respath.count; i++) {
        tr->res_behind_um += 1000 * tr->respath.edges[i]->len_mm;
        if (tr->respath.edges[i] == tr->pctrl.centre.edge)
            break;
    }
    tr->res_behind_um -= tr->pctrl.centre.pos_um;
    tr->res_ahead_um  += tr->pctrl.centre.pos_um;
    for (i++; i < tr->respath.count; i++)
        tr->res_ahead_um += 1000 * tr->respath.edges[i]->len_mm;

    dbglog(&tr->dbglog,
        "train%d reserved %dum ahead, %dum behind",
        tr->train_id,
        tr->res_ahead_um,
        tr->res_behind_um);
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
    tr->pctrl.est_time     = time;
    tr->pctrl.centre.edge  = &sens->edge[TRACK_EDGE_AHEAD];
    tr->pctrl.centre.pos_um =
        tr->pctrl.centre.edge->len_mm * 1000 - ahead_offs_um;

    track_pt_reverse(&tr->pctrl.centre);
    track_pt_advance(&tr->switches, &tr->pctrl.centre, TRAIN_LENGTH_UM / 2);
    track_pt_reverse(&tr->pctrl.centre);

    if (est_pctrl.est_time > time)
        trainsrv_pctrl_advance_ticks(tr, est_pctrl.est_time);

    /* Compute delta */
    tr->pctrl.lastsens = sens;
    assert(TRACK_NODE_DATA(tr->track, tr->pctrl.centre.edge->src, tr->path->node_ix) >= 0);
    tr->pctrl.err_um   = track_pt_distance_path(
        tr->path,
        tr->pctrl.centre,
        est_pctrl.centre);

    /* Might have expected later sensors with too early a timeout. */
    if (tr->pctrl.err_um > 0) {
        tr->path_sensnext = TRACK_NODE_DATA(tr->track, sens, tr->path->node_ix);
        tr->path_sensnext++;
    }

    /* Need to adjust reserved distances */
    tr->res_ahead_um  += tr->pctrl.err_um;
    tr->res_behind_um -= tr->pctrl.err_um;

    /* Print log messages */
    {
        int centre_um, centre_cm, est_centre_um, est_centre_cm;
        int err_um, err_cm;
        bool err_neg;
        centre_um  = tr->pctrl.centre.pos_um;
        centre_cm  = centre_um / 10000;
        centre_um %= 10000;
        est_centre_um  = est_pctrl.centre.pos_um;
        est_centre_cm  = est_centre_um / 10000;
        est_centre_um %= 10000;
        err_um  = tr->pctrl.err_um;
        err_neg = err_um < 0;
        if (err_neg)
            err_um = -err_um;
        err_cm  = err_um / 10000;
        err_um %= 10000;
        dbglog(&tr->dbglog,
            "%s: act. at %s-%d.%04dcm, est. at %s-%d.%04dcm, err=%c%d.%04dcm",
            sens->name,
            tr->pctrl.centre.edge->dest->name,
            centre_cm,
            centre_um,
            est_pctrl.centre.edge->dest->name,
            est_centre_cm,
            est_centre_um,
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
    track_pt_advance_path(tr->path, &tr->pctrl.centre, dist_um);
    tr->res_ahead_um  -= dist_um;
    tr->res_behind_um += dist_um;
}

static void
trainsrv_timer(struct train *tr, int time)
{
    int dt, dist;

    switch (tr->pctrl.state) {
    /* TODO: Estimate position while accelerating/decelerating */
    case PCTRL_STOPPED:
        break;

    case PCTRL_STOPPING:
        dt = time - tr->pctrl.est_time;
        tr->pctrl.est_time = time;
        tr->pctrl.stop_ticks -= dt;
        if (tr->pctrl.stop_ticks < 0) {
            struct track_path *last_path;
            tr->pctrl.state = PCTRL_STOPPED;
            dist = tr->pctrl.stop_um;
            assert(dist >= 0);
            trainsrv_pctrl_advance_um(tr, dist);
            tr->reverse_ok = true;
            last_path = &tr->route.paths[tr->route.n_paths - 1];
            if (tr->path != last_path) {
                tr->path++;
                tcmux_train_speed(&tr->tcmux, tr->train_id, 15);
                track_pt_reverse(&tr->pctrl.centre);
                tr->pctrl.reversed = !tr->pctrl.reversed;
                trainsrv_embark(tr);
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

static bool
trainsrv_track_reserve_edge(struct train *tr, track_edge_t edge, int *want_um)
{
    bool ok;
    int edge_um;
    ok = track_reserve(&tr->res, edge);
    if (!ok)
        return false;
    edge_um = 1000 * edge->len_mm;
    tr->res_ahead_um += edge_um;
    *want_um -= edge_um;
    assert(tr->respath.count < (int)ARRAY_SIZE(tr->respath.edges));
    tr->respath.edges[tr->respath.next++] = edge;
    tr->respath.next %= ARRAY_SIZE(tr->respath.edges);
    tr->respath.count++;
    return true;
}

static bool
trainsrv_track_reserve(struct train *tr)
{
    int base_um, need_um, want_um;
    int last_res_ix, path_ix;
    track_edge_t last_res, end_edges[8];
    struct track_pt end;
    int n_end_edges;
    int i;

    base_um  = polyeval(&tr->calib.stop_um, tr->pctrl.vel_umpt);
    base_um += TRAIN_LENGTH_UM / 2;

    need_um  = base_um + TRAIN_RES_NEED_TICKS * tr->pctrl.vel_umpt;
    if (tr->res_ahead_um >= need_um)
        return true;

    /* Try to reserve an additional want_um ahead. */
    want_um  = base_um + TRAIN_RES_WANT_TICKS * tr->pctrl.vel_umpt;
    want_um -= tr->res_ahead_um;
    assert(want_um >= 0);

    last_res_ix = tr->respath.next - 1;
    if (last_res_ix < 0)
        last_res_ix += ARRAY_SIZE(tr->respath.edges);
    last_res = tr->respath.edges[last_res_ix];
    path_ix = TRACK_NODE_DATA(tr->track, last_res->src, tr->path->node_ix);
    if (path_ix >= 0) {
        while (++path_ix < (int)tr->path->hops) {
            track_edge_t newres = tr->path->edges[path_ix];
            bool ok = trainsrv_track_reserve_edge(tr, newres, &want_um);
            if (!ok) {
                return false;
            } else if (want_um < 0) {
                return true;
            }
        }

        last_res_ix = tr->respath.next - 1;
        if (last_res_ix < 0)
            last_res_ix += ARRAY_SIZE(tr->respath.edges);
        last_res = tr->respath.edges[last_res_ix];
    }

    track_pt_from_node(last_res->dest, &end);
    track_pt_advance_trace(
        &tr->switches,
        &end,
        want_um,
        end_edges,
        &n_end_edges,
        ARRAY_SIZE(end_edges));

    for (i = 0; i < n_end_edges; i++) {
        track_edge_t newres;
        bool ok;
        newres = end_edges[i];
        if (i == 0 && newres == last_res)
            continue;
        ok = trainsrv_track_reserve_edge(tr, newres, &want_um);
        if (!ok)
            return false;
        i++;
    }
    return true;
}

static void
trainsrv_track_release(struct train *tr)
{
    for (;;) {
        track_edge_t earliest;
        int          threshold_um;
        assert(tr->respath.count > 0);
        earliest      = tr->respath.edges[tr->respath.earliest];
        threshold_um  = 1000 * earliest->len_mm;
        threshold_um += TRAIN_RES_GRACE_TICKS * tr->pctrl.vel_umpt;
        threshold_um += TRAIN_LENGTH_UM / 2;
        if (tr->res_behind_um < threshold_um)
            break;

        track_release(&tr->res, earliest);
        tr->res_behind_um -= 1000 * earliest->len_mm;
        tr->respath.earliest++;
        tr->respath.earliest %= ARRAY_SIZE(tr->respath.edges);
        tr->respath.count--;
    }
}

static void
trainsrv_pctrl_on_update(struct train *tr)
{
    /* Release track from behind the train. This runs on the STOPPED update. */
    trainsrv_track_release(tr);

    /* Position control follows. Only while moving! */
    if (tr->pctrl.state != PCTRL_CRUISE && tr->pctrl.state != PCTRL_ACCEL)
        return;

    /* Acquire more track (if necessary) ahead of the train. */
    if (!trainsrv_track_reserve(tr)) {
        /* Failed to get enough track! STOP */
        trainsrv_stop(tr);
        return;
    }

    /* Calculate current stopping point and stop if desired. */
    if (tr->pctrl.state == PCTRL_CRUISE || tr->pctrl.state == PCTRL_ACCEL) {
        int stop_um, end_um;
        stop_um = polyeval(&tr->calib.stop_um, tr->pctrl.vel_umpt);
        assert(TRACK_NODE_DATA(tr->track, tr->pctrl.centre.edge->src, tr->path->node_ix) >= 0);
        end_um = track_pt_distance_path(
            tr->path,
            tr->pctrl.centre,
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
trainsrv_pctrl_check_update(struct train *tr)
{
    if (!tr->pctrl.updated)
        return;
    trainsrv_pctrl_on_update(tr);
    tr->pctrl.updated = false;
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

        assert(TRACK_NODE_DATA(tr->track, tr->pctrl.centre.edge->src, tr->path->node_ix) >= 0);
        sensdist_um = track_pt_distance_path(tr->path, tr->pctrl.centre, sens_pt);
        sensdist_um -= TRAIN_LENGTH_UM / 2;
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
            assert(TRACK_NODE_DATA(tr->track, tr->pctrl.centre.edge->src, tr->path->node_ix) >= 0);
            dist_um  = track_pt_distance_path(tr->path, tr->pctrl.centre, sw_pt);
            dist_um -= TRAIN_LENGTH_UM / 2;
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
    est.centre   = tr->pctrl.centre;
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
