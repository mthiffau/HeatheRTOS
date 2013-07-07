#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "bithack.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "u_tid.h"
#include "train_srv.h"

#include "calib.h"

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
#include "switch_srv.h"
#include "sensor_srv.h"
#include "dbglog_srv.h"

#include "track_pt.h"

#define TRAIN_MINSPEED          4
#define TRAIN_MAXSPEED          14
#define TRAIN_DEFSPEED          8
#define TRAIN_CRAWLSPEED        2

#define TRAIN_TIMER_TICKS       1

/* FIXME these should be looked-up per train */
#define TRAIN_FRONT_OFFS_UM     30000
#define TRAIN_BACK_OFFS_UM      145000
#define TRAIN_LENGTH_UM         225000

#define PCTRL_ACCEL_SENSORS     3
#define PCTRL_STOP_TICKS        300

#define TRAIN_SENSOR_OVERESTIMATE_UM    (40 * 1000)
#define TRAIN_SWITCH_AHEAD_TICKS        50

enum {
    TRAINMSG_SETSPEED,
    TRAINMSG_MOVETO,
    TRAINMSG_STOP,
    TRAINMSG_SENSOR,
    TRAINMSG_SENSOR_TIMEOUT,
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
        uint8_t   speed;
        int       dest_node_id;
        sensors_t sensors[SENSOR_MODULES];
        struct {
            uint8_t minspeed;
            uint8_t maxspeed;
        } vcalib;
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

struct train_pctrl {
    int                       state;
    struct track_pt           ahead, behind;
    int                       pos_time;
    int                       stop_ticks, stop_um;
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

    /* Velocity calibration */
    struct calib              calib;
    int                       calib_total_ticks, calib_total_um;
    int                       last_sensor_ix;
    int                       last_sensor_time;
    int                       ignore_time, lap_count;
    uint8_t                   calib_speed, calib_minspeed, calib_maxspeed;
    bool                      calib_decel;

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
static void trainsrv_moveto(struct train *tr, const struct track_node *dest);
static void trainsrv_stop(struct train *tr);
static void trainsrv_vcalib(struct train *tr, uint8_t minspeed, uint8_t maxspeed);
static void trainsrv_stopcalib(struct train *tr, uint8_t speed);
static void trainsrv_moveto_calib(struct train *tr);
static void trainsrv_calibspeed(struct train *tr, uint8_t speed);
static void trainsrv_sensor_orienting(struct train *tr, track_node_t sens);
static void trainsrv_sensor_vcalib(struct train *tr, int time);
static void trainsrv_sensor_stopcalib(struct train *tr);
static void trainsrv_sensor_pre_vcalib(struct train *tr);
static void trainsrv_sensor_pre_stopcalib(struct train *tr);
static void trainsrv_calib_setup(struct train *tr);
static void trainsrv_sensor_running(struct train *tr, track_node_t sens, int time);
static void trainsrv_sensor(
    struct train *tr, sensors_t sensors[SENSOR_MODULES], int time);
static void trainsrv_start_orienting(struct train *tr);
static void trainsrv_sensor_timeout(struct train *tr, int time);
static void trainsrv_pctrl_advance_um(struct train *tr, int dist_um);
static void trainsrv_timer(struct train *tr, int time);
static void trainsrv_expect_sensor(struct train *tr, track_node_t sens);
static void trainsrv_tryrequest_sensor(struct train *tr);
static void trainsrv_pctrl_check_update(struct train *tr);
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
        case TRAINMSG_VCALIB:
            trainsrv_empty_reply(client);
            trainsrv_vcalib(&tr, msg.vcalib.minspeed, msg.vcalib.maxspeed);
            break;
        case TRAINMSG_STOPCALIB:
            trainsrv_empty_reply(client);
            trainsrv_stopcalib(&tr, msg.speed);
            break;
        case TRAINMSG_ORIENT:
            trainsrv_empty_reply(client);
            if (tr.state == TRAIN_DISORIENTED)
                trainsrv_start_orienting(&tr);
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
    const struct calib *initcalib;
    unsigned i;

    tr->state    = TRAIN_DISORIENTED;
    tr->track    = all_tracks[cfg->track_id];
    tr->train_id = cfg->train_id;
    tr->speed    = TRAIN_DEFSPEED;

    tr->pctrl.state = PCTRL_STOPPED;
    tr->pctrl.pos_time = -1;
    tr->pctrl.lastsens = NULL;
    tr->pctrl.updated  = false;
    tr->estimate_client = -1;

    /* take initial calibration */
    initcalib = initcalib_get(tr->train_id, cfg->track_id);
    if (initcalib != NULL)
        tr->calib = *initcalib;
    else
        memset(&tr->calib, '\0', sizeof (tr->calib));

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
    dbglog(&tr->dbglog, "train%d track%d initial velocities (um/tick): %d %d %d %d %d",
        cfg->train_id,
        cfg->track_id,
        tr->calib.vel_umpt[8],
        tr->calib.vel_umpt[9],
        tr->calib.vel_umpt[10],
        tr->calib.vel_umpt[11],
        tr->calib.vel_umpt[12]);
    dbglog(&tr->dbglog, "train%d track%d stopping distances (um): %d %d %d %d %d",
        cfg->train_id,
        cfg->track_id,
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
trainsrv_moveto(struct train *tr, const struct track_node *dest)
{
    int rc;
    if (tr->state != TRAIN_RUNNING || tr->pctrl.state != PCTRL_STOPPED)
        return;

    /* Get path to follow */
    rc = track_pathfind(tr->track, tr->pctrl.ahead.edge->src, dest, &tr->path);
    if (rc < 0 || tr->path.hops == 0)
        return;

    /* Watch for next sensor on path. */
    tr->path_swnext   = 0;
    tr->pctrl.pos_time = Time(&tr->clock);
    tr->pctrl.updated  = true;

    /* Switch all turnouts up to the second sensor. */
    tcmux_train_speed(&tr->tcmux, tr->train_id, tr->speed);
    /* already TRAIN_RUNNING */
    tr->pctrl.state = PCTRL_ACCEL;
}

static void
trainsrv_vcalib(struct train *tr, uint8_t minspeed, uint8_t maxspeed)
{
    if (tr->state != TRAIN_RUNNING || tr->pctrl.state != PCTRL_STOPPED)
        return; /* ignore */

    /* start calibrating */
    tr->state = TRAIN_PRE_VCALIB;
    tr->calib_minspeed = minspeed == 0 ? TRAIN_MINSPEED : minspeed;
    tr->calib_maxspeed = maxspeed == 0 ? TRAIN_MAXSPEED : maxspeed;
    if (tr->calib_minspeed < TRAIN_MINSPEED)
        tr->calib_minspeed = TRAIN_MINSPEED;
    if (tr->calib_minspeed > TRAIN_MAXSPEED)
        tr->calib_minspeed = TRAIN_MAXSPEED;
    if (tr->calib_maxspeed < TRAIN_MINSPEED)
        tr->calib_maxspeed = TRAIN_MINSPEED;
    if (tr->calib_maxspeed > TRAIN_MAXSPEED)
        tr->calib_maxspeed = TRAIN_MAXSPEED;
    if (tr->calib_maxspeed < tr->calib_minspeed)
        tr->calib_maxspeed = tr->calib_minspeed;
    trainsrv_moveto_calib(tr);
}

static void
trainsrv_stopcalib(struct train *tr, uint8_t speed)
{
    if (tr->state != TRAIN_RUNNING || tr->pctrl.state != PCTRL_STOPPED)
        return; /* ignore */

    /* start calibrating */
    tr->calib_speed = speed;
    tr->state = TRAIN_PRE_STOPCALIB;
    trainsrv_moveto_calib(tr);
}

static void
trainsrv_moveto_calib(struct train *tr)
{
    unsigned i;
    int rc;
    rc = track_pathfind(
        tr->track,
        tr->pctrl.ahead.edge->dest,
        tr->track->calib_sensors[0],
        &tr->path);
    assertv(rc, rc == 0);

    /* Set all switches on path to destination */
    for (i = 0; i < tr->path.n_branches; i++) {
        const struct track_node *branch;
        const struct track_edge *edge;
        bool curved;
        int j;
        branch = tr->path.branches[i];
        j      = TRACK_NODE_DATA(tr->track, branch, tr->path.node_ix);
        assert(j >= 0);
        if ((unsigned)j == tr->path.hops)
            break;
        edge   = tr->path.edges[j];
        curved = edge == &branch->edge[TRACK_EDGE_CURVED];
        tcmux_switch_curve(&tr->tcmux, branch->num, curved);
    }

    trainsrv_expect_sensor(tr, tr->track->calib_sensors[0]);
    tcmux_train_speed(&tr->tcmux, tr->train_id, TRAIN_DEFSPEED);
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
}

static void
trainsrv_calibspeed(struct train *tr, uint8_t speed)
{
    tr->state             = TRAIN_VCALIB;
    tr->calib_speed       = speed;
    tr->last_sensor_ix    = 0;
    tr->ignore_time       = 3;
    tr->lap_count         = 3;
    tr->calib_total_ticks = 0;
    tr->calib_total_um    = 0;
    tr->calib.vel_umpt[speed] = -1;
    trainsrv_expect_sensor(tr, tr->track->calib_sensors[1]);
    tcmux_train_speed(&tr->tcmux, tr->train_id, tr->calib_speed - 1); /* XXX */
    tcmux_train_speed(&tr->tcmux, tr->train_id, tr->calib_speed);
}

static void
trainsrv_sensor_orienting(struct train *tr, track_node_t sensnode)
{
    unsigned i;
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
    tr->pctrl.ahead.edge   = &sensnode->edge[TRACK_EDGE_AHEAD];
    tr->pctrl.ahead.pos_um =
        tr->pctrl.ahead.edge->len_mm * 1000 - TRAIN_FRONT_OFFS_UM;

    /* Behind position */
    tr->pctrl.behind = tr->pctrl.ahead;
    track_pt_reverse(&tr->pctrl.behind);
    track_pt_advance(&tr->switches, &tr->pctrl.behind, NULL, TRAIN_LENGTH_UM);
    track_pt_reverse(&tr->pctrl.behind);
    tr->reverse_ok =
        tr->pctrl.behind.edge ==
        sensnode->reverse->edge[TRACK_EDGE_AHEAD].reverse;

    /* Position estimate as of what time. */
    tr->pctrl.pos_time = Time(&tr->clock);
    tr->pctrl.updated  = true;

    /* Train is now stopped. */
    tr->state       = TRAIN_RUNNING;
    tr->pctrl.state = PCTRL_STOPPED;
}

static void
trainsrv_sensor_vcalib(struct train *tr, int time)
{
    int cur_sensor_ix = (tr->last_sensor_ix + 1) % tr->track->n_calib_sensors;
    if (tr->ignore_time > 0) {
        tr->ignore_time--;
    } else {
        int  x  = tr->track->calib_mm[cur_sensor_ix] * 1000;
        int  dt = time - tr->last_sensor_time;
        tr->calib_total_ticks += dt;
        tr->calib_total_um    += x;
    }

    tr->last_sensor_ix   = cur_sensor_ix;
    tr->last_sensor_time = time;

    if (cur_sensor_ix == 0 && --tr->lap_count == 0) {
        int x = tr->calib_total_um;
        int t = tr->calib_total_ticks;
        tr->calib.vel_umpt[tr->calib_speed] = x / t;
        dbglog(&tr->dbglog, "train%d speed %d%s = %d um/tick",
            tr->train_id,
            tr->calib_speed,
            tr->calib_decel ? "'" : "",
            tr->calib.vel_umpt[tr->calib_speed]);
        if (tr->calib_decel && tr->calib_speed == tr->calib_minspeed) {
            tcmux_train_speed(&tr->tcmux, tr->train_id, 0);
            tr->state = TRAIN_DISORIENTED;
        } else {
            int new_speed = tr->calib_speed + (tr->calib_decel ? -1 : 1);
            if (new_speed > tr->calib_maxspeed) {
                new_speed -= 2;
                tr->calib_decel = true;
            }
            if (new_speed < tr->calib_minspeed) {
                tcmux_train_speed(&tr->tcmux, tr->train_id, 0);
                tr->state = TRAIN_DISORIENTED;
            } else {
                trainsrv_calibspeed(tr, new_speed);
            }
        }
    } else {
        int next = cur_sensor_ix + 1;
        next    %= tr->track->n_calib_sensors;
        trainsrv_expect_sensor(tr, tr->track->calib_sensors[next]);
    }
}

static void
trainsrv_sensor_pre_vcalib(struct train *tr)
{
    trainsrv_calib_setup(tr);
    trainsrv_calibspeed(tr, tr->calib_minspeed);
    tr->calib_decel = false;
}

static void
trainsrv_sensor_pre_stopcalib(struct train *tr)
{
    trainsrv_calib_setup(tr);
    trainsrv_expect_sensor(tr, tr->track->calib_sensors[0]);
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
trainsrv_sensor_running(struct train *tr, track_node_t sens, int time)
{
    struct train_pctrl est_pctrl;

    if (tr->pctrl.state == PCTRL_STOPPING)
        return;

    /* Save original for comparison */
    est_pctrl = tr->pctrl;

    tr->pctrl.updated      = true;
    tr->pctrl.ahead.edge   = &sens->edge[TRACK_EDGE_AHEAD];
    tr->pctrl.ahead.pos_um =
        tr->pctrl.ahead.edge->len_mm * 1000 - TRAIN_FRONT_OFFS_UM;
    if (est_pctrl.pos_time > time) {
        int v  = tr->calib.vel_umpt[tr->speed];
        int dt = est_pctrl.pos_time - time;
        if (tr->pctrl.state == PCTRL_STOPPED)
            v = 0;
        trainsrv_pctrl_advance_um(tr, v * dt);
    }

    /* Compute delta */
    const char *where = NULL;
    tr->pctrl.lastsens = sens;
    if (est_pctrl.ahead.edge == tr->pctrl.ahead.edge) {
        where = "same";
        tr->pctrl.err_um = tr->pctrl.ahead.pos_um - est_pctrl.ahead.pos_um;
    } else if (est_pctrl.ahead.edge->dest == tr->pctrl.ahead.edge->src) {
        where = "prev";
        tr->pctrl.err_um  = tr->pctrl.ahead.pos_um;
        tr->pctrl.err_um -= tr->pctrl.ahead.edge->len_mm * 1000;
        tr->pctrl.err_um -= est_pctrl.ahead.pos_um;
    } else if (est_pctrl.ahead.edge->src == tr->pctrl.ahead.edge->dest) {
        where = "next";
        tr->pctrl.err_um  = tr->pctrl.ahead.pos_um;
        tr->pctrl.err_um += est_pctrl.ahead.edge->len_mm * 1000;
        tr->pctrl.err_um -= est_pctrl.ahead.pos_um;
    } else {
        tr->pctrl.lastsens = NULL;
    }

    /* Transition to cruising if our position estimate is good enough. */
    if (tr->pctrl.lastsens != NULL && tr->pctrl.state == PCTRL_ACCEL) {
        int abs_err_um = tr->pctrl.err_um;
        if (abs_err_um < 0)
            abs_err_um = -abs_err_um;
        if (abs_err_um < 20000)
            tr->pctrl.state = PCTRL_CRUISE;
    }

    /* Print log messages */
    if (tr->pctrl.lastsens == NULL) {
        dbglog(&tr->dbglog, "%s: estimate too far", sens->name);
    } else {
        int ahead_um, ahead_cm, est_ahead_um, est_ahead_cm;
        int err_um, err_cm;
        bool err_neg;
        ahead_um  = tr->pctrl.ahead.pos_um;
        ahead_cm  = ahead_um / 10000;
        ahead_um %= 10000;
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
            "%s: act. at %s-%d.%04dcm, est. on %s edge at %s-%d.%04dcm, err=%c%d.%04dcm",
            sens->name,
            tr->pctrl.ahead.edge->dest->name,
            ahead_cm,
            ahead_um,
            where,
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

    tr->sensor_rdy = true;
    hit = sensors_scan(sensors);
    assert(hit != SENSOR1_INVALID);
    sens = &tr->track->nodes[hit];
    assert(sens->type == TRACK_NODE_SENSOR);

    dbglog(&tr->dbglog, "hit %s", sens->name);

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

    switch (tr->state) {
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
    case TRAIN_RUNNING:
        trainsrv_sensor_running(tr, sens, time);
        break;
    default:
        panic("unexpected sensor reading in train state %d", tr->state);
    }
}

static void
trainsrv_pctrl_advance_um(struct train *tr, int dist_um)
{
    tr->pctrl.updated = true;
    track_pt_advance(&tr->switches, &tr->pctrl.ahead, NULL, dist_um);
}

static void
trainsrv_timer(struct train *tr, int time)
{
    int dt, dist;

    if (tr->state != TRAIN_RUNNING)
        return;

    dt = time - tr->pctrl.pos_time;
    tr->pctrl.pos_time = time;

    switch (tr->pctrl.state) {
    /* TODO: Estimate position while accelerating/decelerating */
    case PCTRL_STOPPED:
        break;

    case PCTRL_STOPPING:
        tr->pctrl.stop_ticks -= dt;
        if (tr->pctrl.stop_ticks < 0) {
            tr->pctrl.state = PCTRL_STOPPED;
            dist = tr->pctrl.stop_um;
            if (dist >= 0)
                trainsrv_pctrl_advance_um(tr, dist);
            else
                tr->state = TRAIN_DISORIENTED; /* lost track of position */
        }
        break;

    case PCTRL_ACCEL:
    case PCTRL_CRUISE:
        dist  = tr->calib.vel_umpt[tr->speed];
        dist *= dt;
        trainsrv_pctrl_advance_um(tr, dist);
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
        for (;;) {
            int min_val;
            min = pqueue_peekmin(&tr->sensor_times);
            if (min == NULL)
                return;
            if (min->key >= tr->pctrl.pos_time)
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

    dbglog(&tr->dbglog, "waiting for %x-%x-%x-%x-%x, timeout=%d",
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
    struct track_pt pos;
    track_node_t branch;
    bool should_switch;
    unsigned i, nsensors;

    if (!tr->pctrl.updated)
        return;

    /* Reply to estimate client if any */
    trainsrv_send_estimate(tr);

    /* Reset updated flag. */
    tr->pctrl.updated = false;

    /* Position control follows. Only while moving! */
    if (tr->state != TRAIN_RUNNING || tr->pctrl.state == PCTRL_STOPPED)
        return;

    /* Calculate current stopping point and stop if desired. */
    if (tr->pctrl.state == PCTRL_CRUISE) {
        bool should_stop;
        pos = tr->pctrl.ahead;
        should_stop = track_pt_advance_path(
            &tr->path,
            &pos,
            tr->path.edges[tr->path.hops - 1]->dest,
            tr->calib.stop_um[tr->speed]);

        if (should_stop)
            trainsrv_stop(tr);
    }

    /* Check for branches ahead. */
    do {
        int sw_um = tr->calib.vel_umpt[tr->speed] * TRAIN_SWITCH_AHEAD_TICKS;
        pos    = tr->pctrl.ahead;
        branch = tr->path.branches[tr->path_swnext];
        should_switch = track_pt_advance_path(&tr->path, &pos, branch, sw_um);
        if (should_switch) {
            int ix;
            track_edge_t edge;
            bool curved;
            ix     = TRACK_NODE_DATA(tr->track, branch, tr->path.node_ix);
            edge   = tr->path.edges[ix];
            curved = edge == &edge->src->edge[TRACK_EDGE_CURVED];
            tcmux_switch_curve(&tr->tcmux, branch->num, curved);
            tr->path_swnext++;
        }
    } while (should_switch);

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
trainsrv_send_estimate(struct train *tr)
{
    struct trainest_reply est;
    int rc;

    assert(tr->pctrl.updated);
    if (tr->estimate_client < 0)
        return;

    est.train_id       = tr->train_id;
    est.ahead_edge_id  = (tr->pctrl.ahead.edge->src - tr->track->nodes) << 1;
    est.ahead_edge_id |= tr->pctrl.ahead.edge - tr->pctrl.ahead.edge->src->edge;
    est.ahead_mm       = tr->pctrl.ahead.pos_um / 1000;
    est.err_mm         = tr->pctrl.err_um / 1000;
    if (tr->pctrl.lastsens != NULL)
        est.lastsens_node_id = tr->pctrl.lastsens - tr->track->nodes;
    else
        est.lastsens_node_id = -1;

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
train_vcalib(struct trainctx *ctx, uint8_t minspeed, uint8_t maxspeed)
{
    struct trainmsg msg;
    int rplylen;
    msg.type = TRAINMSG_VCALIB;
    msg.vcalib.minspeed = minspeed;
    msg.vcalib.maxspeed = maxspeed;
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
    est_out->err_mm   = est.err_mm;
    if (est.lastsens_node_id >= 0)
        est_out->lastsens = &ctx->track->nodes[est.lastsens_node_id];
    else
        est_out->lastsens = NULL;
}

static void
trainsrv_fmt_name(uint8_t train_id, char buf[32])
{
    struct ringbuf rbuf;
    rbuf_init(&rbuf, buf, 32);
    rbuf_printf(&rbuf, "train%d", train_id);
    rbuf_putc(&rbuf, '\0');
}
