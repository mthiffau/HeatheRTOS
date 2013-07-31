#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "xrand.h"
#include "poly.h"
#include "bithack.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "u_tid.h"
#include "switch_srv.h"
#include "track_srv.h"
#include "track_pt.h"
#include "circuit.h"
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
#define TRAIN_ORIENT_ERR_UM             50000  // 5cm
#define TRAIN_RES_NEED_TICKS            10
#define TRAIN_RES_WANT_TICKS            20
#define TRAIN_RES_GRACE_TICKS           20
#define TRAIN_RES_GRACE_UM              30000  // 3cm
#define TRAIN_REROUTE_RETRIES           4
#define TRAIN_STOP_TICKS_EXTRA          100

#define TRAIN_DEF_REV_SLACK_MM          75   // 7.5cm
#define TRAIN_DEF_REV_PENALTY_MM        1000 // 1m
#define TRAIN_DEF_INIT_REV_OK           true
#define TRAIN_DEF_REV_OK                true
#define TRAIN_DEF_WANDER                false

enum {
    TRAINMSG_SET_SPEED,
    TRAINMSG_SET_REV_PENALTY,
    TRAINMSG_SET_REV_SLACK,
    TRAINMSG_SET_INIT_REV_OK,
    TRAINMSG_SET_REV_OK,
    TRAINMSG_MOVETO,
    TRAINMSG_STOP,
    TRAINMSG_SENSOR,
    TRAINMSG_SENSOR_TIMEOUT,
    TRAINMSG_TIMER,
    TRAINMSG_ESTIMATE,
    TRAINMSG_WANDER,
    TRAINMSG_CIRCUIT,
};

enum {
    TRAIN_PARKED,
    TRAIN_ENROUTE,
    TRAIN_CIRCUITING,
    TRAIN_WAITING,
};

enum {
    NOT_STOPPING,
    STOP_AT_DEST,
    STOP_FOR_REQ,
    STOP_NO_TRACK,
};

enum {
    PCTRL_STOPPED,
    PCTRL_ACCEL,
    PCTRL_STOPPING,
    PCTRL_CRUISE,
};

enum {
    PATH_BEFORE,
    PATH_ON,
    PATH_AFTER,
};

struct trainmsg {
    int type;
    int time; /* sensors, timer only */
    union {
        uint8_t         speed;
        int             rev_penalty_mm;
        int             rev_slack_mm;
        bool            init_rev_ok;
        bool            rev_ok;
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
    int                       path_state;
    struct track_pt           centre;
    int                       est_time, accel_start;
    int                       vel_umpt;
    int                       stop_ticks, stop_um, stop_ticks_extra;
    int                       stop_starttime, stop_offstime;
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
    struct trackctx           res;
    struct sensorctx          sensors;
    struct dbglogctx          dbglog;
    struct rand               random;

    /* General train state */
    int                       state;
    const struct track_graph *track;
    uint8_t                   train_id;
    uint8_t                   speed;
    int                       wait_ticks;
    int                       wait_start;
    int                       wait_reroute_retries;
    int                       sensor_window_ticks;

    /* Configuration values */
    struct {
        uint8_t               desired_speed;
        int                   rev_penalty_mm;
        int                   rev_slack_mm;
        bool                  init_rev_ok;
        bool                  rev_ok;
        bool                  wander;
        bool                  circuit;
    } cfg;

    /* Calibration data */
    struct calib              calib;

    /* Timer */
    tid_t                     timer_tid;

    /* Position estimate */
    struct train_pctrl        pctrl;
    tid_t                     estimate_client;

    /* Route to follow */
    struct track_pt           final_dest; /* invalid while TRAIN_PARKED */
    struct track_route        route;
    struct track_path        *path;       /* NULL if no route yet */
    int                       path_swnext;
    int                       path_sensnext;
    bool                      reverse_ok;
    int                       stop_reason;
    bool                      wait_forsubres;

    /* Reservation info */
    struct respath            respath;
};

static int trainsrv_distance_path(
    struct train *tr, const struct train_pctrl *pctrl, struct track_pt whither);
static void trainsrv_init(struct train *tr, struct traincfg *cfg);
static void trainsrv_orient_train(struct train *t);
static void trainsrv_setspeed(struct train *tr, uint8_t speed);
static void trainsrv_swnext_init(struct train *tr);
static void trainsrv_swnext_advance(struct train *tr);
static void trainsrv_choose_wait_time(struct train *tr);
static void trainsrv_waiting(struct train *tr);
static void trainsrv_parked(struct train *tr);
static void trainsrv_move_randomly(struct train *tr);
static void trainsrv_embark(struct train *tr);
static void trainsrv_aboutface(struct train *tr);
static void trainsrv_moveto(struct train *tr, struct track_pt dest);
static void trainsrv_stop(struct train *tr, int why);
static void trainsrv_goto_circuit(struct train *tr);
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
static void trainsrv_pctrl_on_update(struct train *tr);
static void trainsrv_pctrl_check_update(struct train *tr);
static void trainsrv_track_release(struct train *tr);
static bool trainsrv_track_reserve_initial(struct train *tr);
static bool trainsrv_track_reserve_moving(struct train *tr);
static bool trainsrv_track_reserve(
    struct train *tr, int need_um, int want_um, int accel_ticks, int accel_um);
static void trainsrv_pctrl_expect_sensors(struct train *tr);
static void trainsrv_pctrl_switch_turnouts(struct train *tr, bool all);
static void trainsrv_send_estimate(struct train *tr, tid_t client);
static void trainsrv_pctrl_update_path_state(struct train *tr, int delta_um);

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
        case TRAINMSG_SET_SPEED:
            trainsrv_empty_reply(client);
            trainsrv_setspeed(&tr, msg.speed);
            dbglog(&tr.dbglog, "train%d set max speed to %d",
                tr.train_id,
                tr.cfg.desired_speed);
            break;
        case TRAINMSG_SET_REV_PENALTY:
            trainsrv_empty_reply(client);
            tr.cfg.rev_penalty_mm = msg.rev_penalty_mm;
            dbglog(&tr.dbglog, "train%d set reverse penalty to %dmm",
                tr.train_id,
                tr.cfg.rev_penalty_mm);
            assert(tr.cfg.rev_penalty_mm >= 0);
            break;
        case TRAINMSG_SET_REV_SLACK:
            trainsrv_empty_reply(client);
            tr.cfg.rev_slack_mm = msg.rev_slack_mm;
            dbglog(&tr.dbglog, "train%d set reverse slack to %dmm",
                tr.train_id,
                tr.cfg.rev_slack_mm);
            assert(tr.cfg.rev_slack_mm >= 0);
            break;
        case TRAINMSG_SET_INIT_REV_OK:
            trainsrv_empty_reply(client);
            tr.cfg.init_rev_ok = msg.init_rev_ok;
            dbglog(&tr.dbglog, "train%d %sabled reverse at start of path",
                tr.train_id,
                tr.cfg.init_rev_ok ? "en" : "dis");
            break;
        case TRAINMSG_SET_REV_OK:
            trainsrv_empty_reply(client);
            tr.cfg.rev_ok = msg.rev_ok;
            dbglog(&tr.dbglog, "train%d %sabled reverse in path",
                tr.train_id,
                tr.cfg.rev_ok ? "en" : "dis");
            break;
        case TRAINMSG_MOVETO:
            trainsrv_empty_reply(client);
            trainsrv_moveto(&tr, msg.dest);
            break;
        case TRAINMSG_STOP:
            trainsrv_empty_reply(client);
            tr.cfg.wander  = false;
            tr.cfg.circuit = false;
            trainsrv_stop(&tr, STOP_FOR_REQ);
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
        case TRAINMSG_WANDER:
            trainsrv_empty_reply(client);
            tr.cfg.wander = true;
            if (tr.state == TRAIN_PARKED)
                trainsrv_move_randomly(&tr);
            break;
        case TRAINMSG_CIRCUIT:
            trainsrv_empty_reply(client);
            tr.cfg.circuit = true;
            if (tr.state == TRAIN_PARKED)
                trainsrv_goto_circuit(&tr);
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
    tr->path          = NULL;
    tr->sensor_window_ticks = TRAIN_SENSOR_AHEAD_TICKS;

    tr->cfg.desired_speed  = TRAIN_DEFSPEED;
    tr->cfg.rev_penalty_mm = TRAIN_DEF_REV_PENALTY_MM;
    tr->cfg.rev_slack_mm   = TRAIN_DEF_REV_SLACK_MM;
    tr->cfg.init_rev_ok    = TRAIN_DEF_INIT_REV_OK;
    tr->cfg.rev_ok         = TRAIN_DEF_REV_OK;
    tr->cfg.wander         = TRAIN_DEF_WANDER;
    tr->cfg.circuit        = false;

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
    rand_init_time(&tr->random);

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
    tr->cfg.desired_speed = speed;
    /* TODO? maybe change while we're moving? MAYBE */
}

static void
trainsrv_swnext_init(struct train *tr)
{
    tr->path_swnext = 0;
    trainsrv_swnext_advance(tr);
}

static void
trainsrv_swnext_advance(struct train *tr)
{
    if (tr->path_swnext >= 0 && (unsigned)tr->path_swnext >= tr->path->hops + 1)
        return;
    tr->path_swnext++;
    if (tr->path_swnext == 0
        && tr->path->edges[tr->path_swnext]->src->type == TRACK_NODE_MERGE)
        tr->path_swnext++;
    while ((unsigned)tr->path_swnext <= tr->path->hops) {
        int type;
        if ((unsigned)tr->path_swnext < tr->path->hops)
            type = tr->path->edges[tr->path_swnext]->src->type;
        else
            type = tr->path->edges[tr->path->hops - 1]->dest->type;
        if (type == TRACK_NODE_MERGE || type == TRACK_NODE_BRANCH)
            break;
        tr->path_swnext++;
    }
}

static void
trainsrv_choose_wait_time(struct train *tr)
{
    assert(tr->state == TRAIN_WAITING);
    tr->wait_ticks = randrange(&tr->random, 100, 1000);
    tr->wait_start = tr->pctrl.est_time;
    if (tr->wait_forsubres)
        tr->wait_ticks = 30 * 100; /* 30 seconds */
}

static void
trainsrv_waiting(struct train *tr)
{
    if (tr->state == TRAIN_WAITING)
        return;

    tr->state                = TRAIN_WAITING;
    tr->wait_reroute_retries = TRAIN_REROUTE_RETRIES;
    trainsrv_choose_wait_time(tr);
}

static void
trainsrv_parked(struct train *tr)
{
    assert(tr->pctrl.state == PCTRL_STOPPED);
    tr->state = TRAIN_PARKED;
    tr->path  = NULL;
    if (tr->cfg.circuit)
        trainsrv_goto_circuit(tr);
    else if (tr->cfg.wander)
        trainsrv_move_randomly(tr);
}

static void
trainsrv_move_randomly(struct train *tr)
{
    track_node_t dest;
    struct track_pt dest_pt;
    for (;;) {
        uint32_t node_id = randrange(&tr->random, 0, tr->track->n_nodes);
        struct reservation res;
        dest = &tr->track->nodes[node_id];
        if (dest->type != TRACK_NODE_SENSOR)
            continue;
        if (dest->edge[TRACK_EDGE_AHEAD].dest->type == TRACK_NODE_EXIT)
            continue;
        if (dest->reverse->edge[TRACK_EDGE_AHEAD].dest->type == TRACK_NODE_EXIT)
            continue;
        track_query(&tr->res, &dest->edge[TRACK_EDGE_AHEAD], &res);
        if (res.train_id >= 0)
            continue;
        track_query(&tr->res, &dest->reverse->edge[TRACK_EDGE_AHEAD], &res);
        if (res.train_id >= 0)
            continue;
        break;
    }
    track_pt_from_node(dest, &dest_pt);
    trainsrv_moveto(tr, dest_pt);
}

static void
trainsrv_embark(struct train *tr)
{
    assert(tr->pctrl.path_state != PATH_ON
        || TRACK_EDGE_DATA(tr->track,
            tr->pctrl.centre.edge,
            tr->path->edge_ix) >= 0);
    int path_remaining_um = trainsrv_distance_path(tr, &tr->pctrl, tr->path->end);

    /* Find speed we want to use. */
    tr->speed = tr->cfg.desired_speed;
    while (tr->speed >= TRAIN_MINSPEED) {
        int accel_time     = tr->calib.accel_cutoff[tr->speed];
        int accel_end_um   = polyeval(&tr->calib.accel, accel_time);
        int vel_umpt       = tr->calib.vel_umpt[tr->speed];
        int stop_um        = polyeval(&tr->calib.stop_um, vel_umpt);
        int min_allowed_um = accel_end_um + stop_um;
        min_allowed_um    += vel_umpt * TRAIN_ACCEL_STABILIZE_TICKS;
        if (min_allowed_um <= path_remaining_um)
            break;
        tr->speed--;
    }

    /* FIXME clamping the speed to minimum */
    if (tr->speed < TRAIN_MINSPEED)
        tr->speed = TRAIN_MINSPEED;

    /* Determine remaining length of path, and further reduce speed as
     * necessary to obtain an initial reservation. */
    for (;;) {
        if (trainsrv_track_reserve_initial(tr))
            break;
        if (tr->speed <= TRAIN_MINSPEED) {
            trainsrv_waiting(tr);
            return; /* Don't continue starting! There's no track. */
        } else {
            tr->speed--;
        }
    }

    /* Set up motion state */
    tr->pctrl.vel_umpt = 0;
    tr->pctrl.est_time = Time(&tr->clock);
    tr->pctrl.updated  = true;

    tcmux_train_speed(&tr->tcmux, tr->train_id, tr->speed);
    tr->state       = TRAIN_ENROUTE;
    tr->pctrl.state = PCTRL_ACCEL;
    tr->pctrl.accel_start = tr->pctrl.est_time + tr->calib.accel_delay;
    tr->stop_reason = NOT_STOPPING;
    tr->wait_forsubres = false;

    dbglog(&tr->dbglog,
        "train%d moving from %s-%dmm to %s-%dmm at speed %d (v=%d um/tick)",
        tr->train_id,
        tr->path->start.edge->dest->name,
        tr->path->start.pos_um / 1000,
        tr->path->end.edge->dest->name,
        tr->path->end.pos_um / 1000,
        tr->speed,
        tr->calib.vel_umpt[tr->speed]);

    {
        struct ringbuf rbuf;
        char mem[128];
        int i;
        rbuf_init(&rbuf, mem, sizeof (mem));
        i = tr->respath.earliest;
        while (i != tr->respath.next) {
            track_edge_t edge = tr->respath.edges[i];
            rbuf_printf(&rbuf, " %s", edge->src->name);
            i++;
            i %= ARRAY_SIZE(tr->respath.edges);
            if (i == tr->respath.next)
                rbuf_printf(&rbuf, " %s", edge->dest->name);
        }
        rbuf_putc(&rbuf, '\0');
        dbglog(&tr->dbglog, "train%d IR:%s", tr->train_id, mem);
    }
}

static track_edge_t
trainsrv_next_track_edge(struct train *tr, track_edge_t edge)
{
    track_node_t src = edge->dest;
    assert(src->type != TRACK_NODE_EXIT);
    if (src->type != TRACK_NODE_BRANCH)
        return &src->edge[TRACK_EDGE_AHEAD];

    if (switch_iscurved(&tr->switches, src->num))
        return &src->edge[TRACK_EDGE_CURVED];
    else
        return &src->edge[TRACK_EDGE_STRAIGHT];
}

static int
trainsrv_distance_to_edge(
    struct train *tr,
    track_edge_t tgt,
    struct track_pt whence)
{
    track_edge_t cur;
    int i, dist_um;
    dist_um = whence.pos_um;
    cur     = trainsrv_next_track_edge(tr, whence.edge);
    i       = 0;
    while (cur != tgt) {
        dist_um += 1000 * cur->len_mm;
        cur = trainsrv_next_track_edge(tr, cur);
        assertv(i, ++i < 16);
    }
    return dist_um;
}

static int
trainsrv_distance_to_path_start(struct train *tr, struct track_pt whence)
{
    return trainsrv_distance_to_edge(tr, tr->path->edges[0], whence);
}

static int
trainsrv_distance_to_rev_path_end(struct train *tr, struct track_pt whence)
{
    track_edge_t last;
    last = tr->path->edges[tr->path->hops - 1]->reverse;
    track_pt_reverse(&whence);
    return -trainsrv_distance_to_edge(tr, last, whence);
}

static int
trainsrv_distance_path(
    struct train *tr,
    const struct train_pctrl *pctrl,
    struct track_pt whither)
{
    int i, extra_um;
    struct track_pt whence;
    whence = pctrl->centre;
    switch (pctrl->path_state) {
    case PATH_ON:
        i = TRACK_EDGE_DATA(tr->track, whence.edge, tr->path->edge_ix);
        if (i < 0)
            panic(
                "panic! train_srv.c:572: assertion failed: i >= 0 "
                "[train%d, whence.edge=%s->%s, i=%d",
                tr->train_id,
                whence.edge->src->name,
                whence.edge->dest->name,
                i);
        //assertv(i, i >= 0);
        extra_um = 0;
        break;
    case PATH_BEFORE:
        i = TRACK_EDGE_DATA(tr->track, whence.edge, tr->path->edge_ix);
        assertv(i, i < 0);
        extra_um      = trainsrv_distance_to_path_start(tr, whence);
        whence.edge   = tr->path->edges[0];
        whence.pos_um = 1000 * whence.edge->len_mm - 1;
        break;
    case PATH_AFTER:
        i = TRACK_EDGE_DATA(tr->track, whence.edge, tr->path->edge_ix);
        assertv(i, i < 0);
        extra_um      = trainsrv_distance_to_rev_path_end(tr, whence);
        whence.edge   = tr->path->edges[tr->path->hops - 1];
        whence.pos_um = 0;
        break;
    default:
        panic("panic! unknown pctrl->path_state %d", pctrl->path_state);
    }
    return extra_um + track_pt_distance_path(tr->path, whence, whither);
}

static void
trainsrv_aboutface(struct train *tr)
{
    int i, j;

    /* Check for edges to release before we screw with them.
     * Otherwise, we may not pick up a position update causing
     * this reversal before reversing all of this state. */
    trainsrv_track_release(tr);

    /* Reverse the train. */
    tcmux_train_speed(&tr->tcmux, tr->train_id, 15);
    track_pt_reverse(&tr->pctrl.centre);
    tr->pctrl.reversed = !tr->pctrl.reversed;

    /* Reverse path before/after state. */
    assert(tr->pctrl.path_state != PATH_BEFORE);
    if (tr->pctrl.path_state == PATH_AFTER)
        tr->pctrl.path_state = PATH_BEFORE;

    /* Reverse order of reserved edge path. */
    i = tr->respath.earliest;
    j = tr->respath.next - 1;
    if (j < 0)
        j += ARRAY_SIZE(tr->respath.edges);

    while (i != j) {
        track_edge_t tmp_edge;
        bool tmp_is_sub;
        tmp_edge = tr->respath.edges[i];
        tr->respath.edges[i] = tr->respath.edges[j];
        tr->respath.edges[j] = tmp_edge;

        tmp_is_sub = tr->respath.is_sub[i];
        tr->respath.is_sub[i] = tr->respath.is_sub[j];
        tr->respath.is_sub[j] = tmp_is_sub;

        i++;
        i %= ARRAY_SIZE(tr->respath.edges);
        if (i == j)
            break;
        j--;
        if (j < 0)
            j += ARRAY_SIZE(tr->respath.edges);
    }

    i = tr->respath.earliest;
    while (i != tr->respath.next) {
        tr->respath.edges[i] = tr->respath.edges[i]->reverse;
        i++;
        i %= ARRAY_SIZE(tr->respath.edges);
    }
}

static bool
trainsrv_route_too_short(struct train *tr)
{
    int i, len_mm;
    len_mm = 0;
    for (i = 0; i < tr->route.n_paths; i++)
        len_mm += tr->route.paths[i].len_mm;
    return len_mm < 500;
}

static void
trainsrv_moveto(struct train *tr, struct track_pt dest)
{
    struct track_routespec rspec;
    int rc;
    if (tr->state != TRAIN_PARKED && tr->state != TRAIN_WAITING)
        return;

    /* Reset waiting reason */
    tr->wait_forsubres = false;

    /* Save destination (in case of retry) */
    tr->final_dest = dest;

    /* Get path to follow */
    track_routespec_init(&rspec);
    rspec.track        = tr->track;
    rspec.switches     = &tr->switches;
    rspec.res          = &tr->res;
    rspec.train_id     = tr->train_id;
    rspec.src_centre   = tr->pctrl.centre;
    rspec.err_um       = 50000; /* FIXME hardcoded 5cm */
    rspec.init_rev_ok  = tr->cfg.init_rev_ok && tr->reverse_ok;
    rspec.rev_ok       = tr->cfg.rev_ok;
    rspec.rev_penalty_mm = tr->cfg.rev_penalty_mm;
    rspec.rev_slack_mm = tr->cfg.rev_slack_mm;
    rspec.train_len_um = TRAIN_LENGTH_UM; /* FIXME */
    rspec.dest         = dest;
    if (tr->cfg.circuit) {
        rspec.rev_ok      = false;
        rspec.dest_unidir = true;
    }

    rc = track_routefind(&rspec, &tr->route);
    if (rc < 0 || tr->route.n_paths == 0 || tr->route.paths[0].hops == 0
        || (tr->cfg.wander && trainsrv_route_too_short(tr))) {
        dbglog(&tr->dbglog, "failed to get route to %s-%dmm",
            dest.edge->dest->name,
            dest.pos_um / 1000);
        trainsrv_waiting(tr);
        return;
    }

    tr->path = &tr->route.paths[0];

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

    /* We're on the path, since it was generated from our edge. */
    tr->pctrl.path_state = PATH_ON;

    /* Initial reverse if necessary */
    if (tr->path->edges[0] == rspec.src_centre.edge->reverse)
        trainsrv_aboutface(tr); /* This fixes up path_{ahead,behind}_um */

    /* Starting a new path. */
    trainsrv_swnext_init(tr);
    tr->path_sensnext = 1; /* Ignore first sensor if it is the source
                            * of the first edge on the path */

    /* Go! */
    trainsrv_embark(tr);
}

static void
trainsrv_release_early_sub(struct train *tr)
{
    int i, last_is_sub;
    last_is_sub = -1;
    i = tr->respath.earliest;
    while (i != tr->respath.next) {
        if (tr->respath.is_sub[i])
            last_is_sub = i;
        i++;
        i %= ARRAY_SIZE(tr->respath.edges);
    }
    if (last_is_sub < 0)
        return;
    for (;;) {
        bool last = tr->respath.earliest == last_is_sub;
        track_edge_t edge = tr->respath.edges[tr->respath.earliest];
        if (tr->respath.is_sub[tr->respath.earliest])
            track_subrelease(&tr->res, edge, -1);
        else
            track_release(&tr->res, edge);
        tr->respath.earliest++;
        tr->respath.earliest %= ARRAY_SIZE(tr->respath.edges);
        if (last)
            break;
    }
}

static void
trainsrv_stop(struct train *tr, int why)
{
    static const char * const reason_names[] = {
        "NOT_STOPPING",
        "STOP_AT_DEST",
        "STOP_FOR_REQ",
        "STOP_NO_TRACK"
    };

    int stop_pos_um;
    tr->stop_reason = why;
    if (tr->pctrl.state == PCTRL_STOPPING || tr->pctrl.state == PCTRL_STOPPED)
        return; /* ignore */

    tcmux_train_speed(&tr->tcmux, tr->train_id, 0);

    stop_pos_um =
        polyeval(&tr->calib.stop_um, tr->calib.vel_umpt[TRAIN_MAXSPEED]);
    tr->pctrl.state      = PCTRL_STOPPING;
    tr->pctrl.stop_um    = polyeval(&tr->calib.stop_um, tr->pctrl.vel_umpt);
    tr->pctrl.stop_starttime = tr->pctrl.est_time;
    tr->pctrl.stop_offstime  = polyinv(&tr->calib.stop,
        stop_pos_um - tr->pctrl.stop_um,
        0.f,    /* initial guess is time 0 */
        500.f,  /* within a millimetre is good enough */
        5);     /* use no more than 5 iterations */
    tr->pctrl.stop_ticks = -tr->pctrl.stop_offstime + polyinv(&tr->calib.stop,
        stop_pos_um,
        0.f,    /* initial guess is time 0 */
        500.f,  /* within a millimetre is good enough */
        5);     /* use no more than 5 iterations */
    tr->pctrl.stop_ticks_extra = TRAIN_STOP_TICKS_EXTRA;

    dbglog(&tr->dbglog, "train%d expecting to take %d ticks, %d um to stop",
        tr->train_id,
        tr->pctrl.stop_ticks,
        (int)(stop_pos_um - polyeval(&tr->calib.stop, tr->pctrl.stop_offstime)));

    dbglog(&tr->dbglog, "train%d stopping from %s-%dum, v=%dum/cs, d=%dum, reason=%s",
        tr->train_id,
        tr->pctrl.centre.edge->dest->name,
        tr->pctrl.centre.pos_um,
        tr->pctrl.vel_umpt,
        tr->pctrl.stop_um,
        reason_names[why]);

    /* Release any subreserved edges at the beginning of respath.
     * Otherwise, we wouldn't release them until we were done stopping,
     * which is sometimes a little bit too slow. */
    trainsrv_release_early_sub(tr);
}

static void
trainsrv_goto_circuit(struct train *tr)
{
    struct track_pt start_pt;
    track_pt_from_node(tr->track->calib_sensors[0], &start_pt);
    trainsrv_moveto(tr, start_pt);
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

    /* Train is now parked. */
    tr->pctrl.state = PCTRL_STOPPED;
    trainsrv_parked(tr);

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
        bool success = track_reserve(&tr->res, tr->respath.edges[i]) == RESERVE_SUCCESS;
        if (success)
            continue;

        dbglog(&tr->dbglog,
            "train%d failed to reserve initial position",
            tr->train_id);
        while (--i >= 0)
            track_release(&tr->res, tr->respath.edges[i]);
        Exit();
    }

    for (i = 0; i < (int)ARRAY_SIZE(tr->respath.is_sub); i++)
        tr->respath.is_sub[i] = false;
}

static void
trainsrv_sensor_running(struct train *tr, track_node_t sens, int time)
{
    struct train_pctrl est_pctrl;
    int ahead_offs_um, sens_path_ix;

    if (tr->pctrl.state == PCTRL_STOPPING || tr->pctrl.state == PCTRL_STOPPED)
        return;

    /* Reject sensors not on our path. These are the result of a too-large
     * sensor timeout window. */
    sens_path_ix = TRACK_EDGE_DATA(
        tr->track,
        &sens->edge[TRACK_EDGE_AHEAD],
        tr->path->edge_ix);
    if (sens_path_ix < 0 || (unsigned)sens_path_ix >= tr->path->hops)
        return;

    /* Save original for comparison */
    est_pctrl = tr->pctrl;
    ahead_offs_um =
        tr->pctrl.reversed ? TRAIN_BACK_OFFS_UM : TRAIN_FRONT_OFFS_UM;

    tr->pctrl.updated      = true;
    tr->pctrl.est_time     = time;
    tr->pctrl.centre.edge  = &sens->edge[TRACK_EDGE_AHEAD];
    tr->pctrl.centre.pos_um = tr->pctrl.centre.edge->len_mm * 1000;

    trainsrv_pctrl_advance_um(tr, ahead_offs_um - TRAIN_LENGTH_UM / 2);
    if (est_pctrl.est_time > time)
        trainsrv_pctrl_advance_ticks(tr, est_pctrl.est_time);

    /* If off of path, don't fall into the trap! */
    if (TRACK_EDGE_DATA(tr->track, tr->pctrl.centre.edge, tr->path->edge_ix) < 0)
        return;

    /* Compute delta */
    assert(est_pctrl.path_state != PATH_ON
        || TRACK_EDGE_DATA(tr->track, est_pctrl.centre.edge, tr->path->edge_ix) >= 0);
    tr->pctrl.lastsens = sens;
    tr->pctrl.err_um   =
        -trainsrv_distance_path(tr, &est_pctrl, tr->pctrl.centre);

    /* Might have expected later sensors with too early a timeout. */
    if (tr->pctrl.err_um > 0)
        tr->path_sensnext = sens_path_ix + 1;

    trainsrv_pctrl_update_path_state(tr, -tr->pctrl.err_um);

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
    (void)time;
    tr->sensor_window_ticks += tr->sensor_window_ticks / 5;
}

static void
trainsrv_sensor(struct train *tr, sensors_t sensors[SENSOR_MODULES], int time)
{
    const struct track_node *sens;
    sensor1_t hit;

    tr->sensor_window_ticks = TRAIN_SENSOR_AHEAD_TICKS;

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
    int dist_um, prev_dt, dt, cutoff;
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
    case PCTRL_STOPPING:
        cutoff = tr->pctrl.est_time + tr->pctrl.stop_ticks;
        if (when > cutoff)
            when = cutoff;
        dt = when - tr->pctrl.stop_starttime + tr->pctrl.stop_offstime;
        prev_dt  = tr->pctrl.est_time;
        prev_dt -= tr->pctrl.stop_starttime;
        prev_dt += tr->pctrl.stop_offstime;
        dist_um  = (int)(polyeval(&tr->calib.stop, dt)
            - polyeval(&tr->calib.stop, prev_dt));
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
    case PCTRL_STOPPING:
        if (when >= tr->pctrl.est_time + tr->pctrl.stop_ticks)
            vel_umpt = 0;
        else {
            struct poly v;
            dt = when - tr->pctrl.stop_starttime + tr->pctrl.stop_offstime;
            polydiff(&tr->calib.stop, &v);
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
trainsrv_pctrl_update_path_state(struct train *tr, int delta_um)
{
    static const char * const names[] = {
        "PATH_BEFORE", "PATH_ON", "PATH_AFTER"
    };
    track_edge_t centre_edge;
    int centre_ix;
    int orig_state;
    orig_state  = tr->pctrl.path_state;
    centre_edge = tr->pctrl.centre.edge;
    centre_ix   = TRACK_EDGE_DATA(tr->track, centre_edge, tr->path->edge_ix);
    if (centre_ix >= 0 && (unsigned)centre_ix < tr->path->hops)
        tr->pctrl.path_state = PATH_ON;
    else if (orig_state == PATH_ON)
        tr->pctrl.path_state = delta_um >= 0 ? PATH_AFTER : PATH_BEFORE;

    if (tr->pctrl.path_state != orig_state) {
        dbglog(&tr->dbglog, "train%d changed from %s to %s at %s-%dmm",
            tr->train_id,
            names[orig_state],
            names[tr->pctrl.path_state],
            tr->pctrl.centre.edge->dest->name,
            tr->pctrl.centre.pos_um / 1000);
    }
}

static void
trainsrv_pctrl_advance_um(struct train *tr, int dist_um)
{
    tr->pctrl.updated = true;
    track_pt_advance_path(&tr->switches, tr->path, &tr->pctrl.centre, dist_um);
    trainsrv_pctrl_update_path_state(tr, dist_um);
}

static void
trainsrv_timer(struct train *tr, int time)
{
    int dt;
    int extra_ticks = 0;

    if (tr->state == TRAIN_WAITING) {
        int dt = time - tr->wait_start;
        if (dt < tr->wait_ticks) {
            if (tr->path != NULL && dt % 10 == 0)
                trainsrv_embark(tr);
        } else {
            dbglog(&tr->dbglog, "train%d re-routing", tr->train_id);
            tr->pctrl.est_time += dt;
            trainsrv_moveto(tr, tr->final_dest);
            if (tr->state == TRAIN_WAITING) {
                /* Re-routing failed */
                if (tr->wait_reroute_retries <= 0) {
                    dbglog(&tr->dbglog,
                        "train%d re-routing failed, giving up",
                        tr->train_id);
                    trainsrv_parked(tr);
                } else {
                    dbglog(&tr->dbglog,
                        "train%d re-routing failed, retrying",
                        tr->train_id);
                    trainsrv_choose_wait_time(tr);
                    tr->wait_reroute_retries--;
                }
            }
        }
        return;
    }

    switch (tr->pctrl.state) {
    /* TODO: Estimate position while accelerating/decelerating */
    case PCTRL_STOPPED:
        break;

    case PCTRL_ACCEL:
    case PCTRL_CRUISE:
        trainsrv_pctrl_advance_ticks(tr, time);
        break;

    case PCTRL_STOPPING:
        dt = time - tr->pctrl.est_time;
        trainsrv_pctrl_advance_ticks(tr, time);
        tr->pctrl.stop_ticks -= dt;
        if (tr->pctrl.stop_ticks < 0) {
            extra_ticks = -tr->pctrl.stop_ticks;
            tr->pctrl.stop_ticks = 0;
            dt -= extra_ticks;
            tr->pctrl.stop_ticks_extra -= extra_ticks;
        }
        if (tr->pctrl.stop_ticks_extra < 0) {
            struct track_path *last_path;
            int reason = tr->stop_reason;
            tr->stop_reason = NOT_STOPPING;
            tr->pctrl.state = PCTRL_STOPPED;
            tr->pctrl.vel_umpt = 0;
            tr->reverse_ok = true;
            dbglog(&tr->dbglog,
                "train%d stopped at %s-%dmm",
                tr->train_id,
                tr->pctrl.centre.edge->dest->name,
                tr->pctrl.centre.pos_um / 1000);
            assert(reason != NOT_STOPPING);
            switch (reason) {
            case STOP_FOR_REQ:
                trainsrv_parked(tr);
                break;
            case STOP_AT_DEST:
                last_path = &tr->route.paths[tr->route.n_paths - 1];
                if (tr->path == last_path)
                    trainsrv_parked(tr);
                else {
                    tr->path++;
                    /* Starting a new path. */
                    trainsrv_swnext_init(tr);
                    tr->path_sensnext = 1; /* Ignore first sensor if it is the source
                                            * of the first edge on the path */
                    trainsrv_aboutface(tr);
                    trainsrv_embark(tr);
                }
                break;
            case STOP_NO_TRACK:
                trainsrv_waiting(tr);
                break;
            }
        }
        break;

    default:
        panic("bad train position control state %d", tr->pctrl.state);
    }
}

struct sens_param {
    struct sensorctx ctx;
    track_node_t sens;
    int window_ticks;
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

    rc = sensor_wait(&sp.ctx, sensors, 2 * sp.window_ticks, &time);
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
    sp.window_ticks = tr->sensor_window_ticks;

    worker = Create(PRIORITY_TRAIN - 1, &sensor_worker);
    assert(worker >= 0);

    replylen = Send(worker, &sp, sizeof(sp), NULL, 0);
    assertv(replylen, replylen == 0);
}

static bool
trainsrv_track_reserve_edge(
    struct train *tr,
    track_edge_t edge,
    int clearance,
    int *want_um,
    bool *is_sub)
{
    bool ok;
    int edge_um;
    if (tr->state == TRAIN_CIRCUITING) {
        ok = track_subreserve(&tr->res, edge, -1);
        *is_sub = true;
    } else {
        int rc = track_reserve(&tr->res, edge);
        *is_sub = false;
        if (rc == RESERVE_SUCCESS) {
            ok = true;
        } else if (rc == RESERVE_FAILURE) {
            ok = false;
        } else {
            assert(rc == RESERVE_SOFTFAIL);
            ok = track_subreserve(&tr->res, edge, clearance + 200);
            *is_sub = true;
        }
    }
    if (!ok)
        return false;

    edge_um = 1000 * edge->len_mm;
    *want_um -= edge_um;
    assert(tr->respath.count < (int)ARRAY_SIZE(tr->respath.edges));
    tr->respath.is_sub[tr->respath.next]  = *is_sub;
    tr->respath.edges[tr->respath.next++] = edge;
    tr->respath.next %= ARRAY_SIZE(tr->respath.edges);
    tr->respath.count++;
    return true;
}

static bool
trainsrv_track_reserve_initial(struct train *tr)
{
    int need_um, orig_count, accel_ticks, accel_um;
    bool success;
    need_um  = polyeval(&tr->calib.accel, tr->calib.accel_cutoff[tr->speed]);
    need_um += polyeval(&tr->calib.stop_um, tr->calib.vel_umpt[tr->speed]);
    need_um += TRAIN_RES_GRACE_UM;
    need_um += TRAIN_LENGTH_UM / 2;
    orig_count = tr->respath.count;
    accel_ticks  = tr->calib.accel_cutoff[tr->speed];
    accel_um     = polyeval(&tr->calib.accel, accel_ticks);
    accel_ticks += tr->calib.accel_delay;
    success = trainsrv_track_reserve(tr, need_um, need_um, accel_ticks, accel_um);
    if (!success) {
        int inc_count = tr->respath.count - orig_count;
        int i;
        i = 0;
        while (i < inc_count) {
            track_edge_t edge;
            tr->respath.next--;
            if (tr->respath.next < 0)
                tr->respath.next += ARRAY_SIZE(tr->respath.edges);
            edge = tr->respath.edges[tr->respath.next];
            if (tr->respath.is_sub[tr->respath.next])
                track_subrelease(&tr->res, edge, -1);
            else
                track_release(&tr->res, edge);
            i++;
        }
        tr->respath.count = orig_count;
    }
    return success;
}

static bool
trainsrv_track_reserve_moving(struct train *tr)
{
    int base_um, need_um, want_um, accel_ticks, accel_um;
    bool success;
    assert(tr->pctrl.state != PCTRL_STOPPING);
    assert(tr->pctrl.state != PCTRL_STOPPED);
    if (tr->state == TRAIN_CIRCUITING) {
        need_um = TRAIN_LENGTH_UM / 2 + TRAIN_RES_NEED_TICKS * tr->pctrl.vel_umpt;
        want_um = need_um;
    } else {
        base_um  = polyeval(&tr->calib.stop_um, tr->pctrl.vel_umpt);
        base_um += TRAIN_LENGTH_UM / 2;
        need_um  = base_um + TRAIN_RES_NEED_TICKS * tr->pctrl.vel_umpt;
        want_um  = base_um + TRAIN_RES_WANT_TICKS * tr->pctrl.vel_umpt;
    }
    if (tr->pctrl.state == PCTRL_CRUISE) {
        accel_ticks = 0;
        accel_um    = 0;
    } else {
        assert(tr->pctrl.state == PCTRL_ACCEL);
        accel_ticks  = tr->pctrl.accel_start + tr->calib.accel_cutoff[tr->speed];
        accel_um     = trainsrv_predict_dist_um(tr, accel_ticks);
        accel_ticks -= tr->pctrl.est_time;
    }
    success = trainsrv_track_reserve(tr, need_um, want_um, accel_ticks, accel_um);
    if (!success) {
        int i;
        i = tr->respath.next;
        for (;;) {
            track_edge_t edge;
            i--;
            if (i < 0)
                i += ARRAY_SIZE(tr->respath.edges);
            if (!tr->respath.is_sub[i])
                break;
            edge = tr->respath.edges[i];
            track_subrelease(&tr->res, edge, -1);
            tr->respath.next = i;
        }
    }
    return success;
}

static bool
trainsrv_track_reserve(
    struct train *tr,
    int need_um,
    int want_um,
    int accel_ticks,
    int accel_um)
{
    int res_ahead_um, cur_edge_ahead_um;
    int last_res_ix, path_ix;
    track_edge_t last_res, end_edges[8];
    struct track_pt end;
    int n_end_edges;
    int i;

    res_ahead_um = 0;
    i = tr->respath.next;
    do {
        i--;
        if (i < 0)
            i += ARRAY_SIZE(tr->respath.edges);
        track_edge_t edge = tr->respath.edges[i];
        if (edge == tr->pctrl.centre.edge) {
            res_ahead_um += tr->pctrl.centre.pos_um;
            break;
        }
        res_ahead_um += 1000 * edge->len_mm;
    } while (i != tr->respath.earliest);

    if (res_ahead_um >= need_um)
        return true;

    /* Try to reserve an additional want_um ahead. */
    cur_edge_ahead_um = res_ahead_um + TRAIN_LENGTH_UM / 2 - accel_um;
    want_um -= res_ahead_um;
    assert(want_um >= 0);

    last_res_ix = tr->respath.next - 1;
    if (last_res_ix < 0)
        last_res_ix += ARRAY_SIZE(tr->respath.edges);
    last_res = tr->respath.edges[last_res_ix];
    path_ix = TRACK_EDGE_DATA(tr->track, last_res, tr->path->edge_ix);
    if (path_ix >= 0) {
        while (++path_ix < (int)tr->path->hops) {
            track_edge_t newres = tr->path->edges[path_ix];
            bool ok, is_sub;
            int  clearance_ticks;
            cur_edge_ahead_um += 1000 * newres->len_mm;
            clearance_ticks    = cur_edge_ahead_um;
            clearance_ticks   /= tr->calib.vel_umpt[tr->speed];
            clearance_ticks   += accel_ticks;
            ok = trainsrv_track_reserve_edge(
                tr,
                newres,
                clearance_ticks,
                &want_um,
                &is_sub);
            if (!ok) {
                return false;
            } else if (want_um < 0) {
                if (is_sub && tr->state != TRAIN_CIRCUITING) {
                    want_um = TRAIN_LENGTH_UM / 2 + TRAIN_RES_GRACE_UM;
                } else {
                    return true;
                }
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
        bool ok, is_sub;
        newres = end_edges[i];
        if (i == 0 && newres == last_res)
            continue;
        ok = trainsrv_track_reserve_edge(tr, newres, 1000000, &want_um, &is_sub);
        if (!ok)
            return false;
        assert(!is_sub || tr->state == TRAIN_CIRCUITING);
        i++;
    }
    return true;
}

static void
trainsrv_track_release(struct train *tr)
{
    int i, res_behind_um;
    res_behind_um = 0;
    i = tr->respath.earliest;
    while (i != tr->respath.next) {
        track_edge_t edge = tr->respath.edges[i];
        res_behind_um += 1000 * edge->len_mm;
        if (edge == tr->pctrl.centre.edge) {
            res_behind_um -= tr->pctrl.centre.pos_um;
            break;
        }
        i++;
        i %= ARRAY_SIZE(tr->respath.edges);
    }

    for (;;) {
        track_edge_t earliest;
        int          threshold_um;
        assert(tr->respath.count > 0);
        earliest      = tr->respath.edges[tr->respath.earliest];
        threshold_um  = 1000 * earliest->len_mm;
        threshold_um += TRAIN_RES_GRACE_TICKS * tr->pctrl.vel_umpt;
        threshold_um += TRAIN_RES_GRACE_UM;
        threshold_um += TRAIN_LENGTH_UM / 2;
        if (res_behind_um < threshold_um)
            break;

        if (!tr->respath.is_sub[tr->respath.earliest]) {
            track_release(&tr->res, earliest);
        } else {
            int clearance = -1;
            if (tr->state == TRAIN_CIRCUITING) {
                struct track_pt src_pt;
                track_pt_from_node(earliest->src, &src_pt);
                int dist = trainsrv_distance_path(tr, &tr->pctrl, src_pt);
                if (dist < 0)
                    dist += 1000 * tr->path->len_mm;
                clearance = dist / tr->pctrl.vel_umpt;
                dbglog(&tr->dbglog,
                    "train%d subrelease %s->%s with clearance %d ticks",
                    tr->train_id,
                    earliest->src->name,
                    earliest->dest->name,
                    clearance);
            }
            track_subrelease(&tr->res, earliest, clearance);
        }
        res_behind_um -= 1000 * earliest->len_mm;
        tr->respath.earliest++;
        tr->respath.earliest %= ARRAY_SIZE(tr->respath.edges);
        tr->respath.count--;
    }
}

static void
trainsrv_rotate_path(struct train *tr)
{
    /* Rotate circuit path so we never get to the end. */
    struct track_pt pt = tr->pctrl.centre;
    track_edge_t edges[TRACK_EDGES_MAX];
    int k, n;
    unsigned i;

    track_pt_advance_path(
        &tr->switches,
        tr->path,
        &pt,
        -1000 * (int)tr->path->len_mm / 4);
    dbglog(&tr->dbglog,
        "new path starting edge is %s->%s at %dum ahead of train at %s-%dmm",
        pt.edge->src->name,
        pt.edge->dest->name,
        -1000 * (int)tr->path->len_mm / 4,
        tr->pctrl.centre.edge->dest->name,
        tr->pctrl.centre.pos_um / 1000);
    k = TRACK_EDGE_DATA(tr->track, pt.edge, tr->path->edge_ix);
    n = tr->path->hops - k;
    memcpy(edges, tr->path->edges, tr->path->hops * sizeof (edges[0]));
    memcpy(tr->path->edges, &edges[k], n * sizeof (edges[0]));
    memcpy(&tr->path->edges[n], &edges[0], k * sizeof (edges[0]));

    tr->path->start.edge   = tr->path->edges[0];
    tr->path->start.pos_um = 1000 * tr->path->start.edge->len_mm - 1;
    tr->path->end.edge     = tr->path->edges[tr->path->hops - 1];
    tr->path->end.pos_um   = 0;

    for (i = 0; i < tr->path->hops; i++)
        TRACK_EDGE_DATA(tr->track, tr->path->edges[i], tr->path->edge_ix) = i;

    if ((unsigned)tr->path_swnext > tr->path->hops)
        tr->path_swnext--;
    tr->path_swnext   += n;
    tr->path_swnext   %= tr->path->hops;
    tr->path_swnext--;
    trainsrv_swnext_advance(tr);
    tr->path_sensnext += n;
    tr->path_sensnext %= tr->path->hops;
    tr->final_dest     = tr->path->end;

    /* log new (rotated) path */
    {
        struct ringbuf rbuf;
        char mem[512];
        rbuf_init(&rbuf, mem, sizeof (mem));
        rbuf_printf(&rbuf, "train%d rotated:", tr->train_id);
        for (i = 0; i < tr->path->hops; i++) {
            track_edge_t edge = tr->path->edges[i];
            rbuf_printf(&rbuf, " %s-%s", edge->src->name, edge->dest->name);
            if (rbuf.len > 100) {
                rbuf_putc(&rbuf, '\0');
                dbglog(&tr->dbglog, "%s", mem);
                rbuf_init(&rbuf, mem, sizeof (mem));
                rbuf_printf(&rbuf, "train%d rotated (cont):", tr->train_id);
            }
        }
        rbuf_putc(&rbuf, '\0');
        dbglog(&tr->dbglog, "%s", mem);
        dbglog(&tr->dbglog, "train%d path_swnext=%d, path_sensnext=%d",
            tr->train_id,
            tr->path_swnext,
            tr->path_sensnext);
    }
}

static void
trainsrv_pctrl_on_update(struct train *tr)
{
    /* Release track from behind the train. This runs on the STOPPED update. */
    trainsrv_track_release(tr);

    /* Position control follows. Only while moving! */
    if (tr->pctrl.state == PCTRL_STOPPED)
        return;

    /* Acquire more track (if necessary) ahead of the train. */
    if (tr->pctrl.state != PCTRL_STOPPING) {
        if (!trainsrv_track_reserve_moving(tr)) {
            /* Failed to get enough track! STOP */
            trainsrv_stop(tr, STOP_NO_TRACK);
            return;
        }

        /* Calculate current stopping point and stop if desired. */
        int stop_um, end_um;
        stop_um = polyeval(&tr->calib.stop_um, tr->pctrl.vel_umpt);
        assert(tr->pctrl.path_state != PATH_ON
            || TRACK_EDGE_DATA(tr->track,
                tr->pctrl.centre.edge,
                tr->path->edge_ix) >= 0);
        end_um = trainsrv_distance_path(tr, &tr->pctrl, tr->path->end);
        if (end_um <= stop_um) {
            if (tr->state == TRAIN_CIRCUITING) {
                trainsrv_rotate_path(tr);
            } else if (tr->cfg.circuit) {
                unsigned i;
                int j;
                mkcircuit(tr->track, &tr->route);
                tr->path = &tr->route.paths[0];
                trainsrv_swnext_init(tr);
                tr->path_sensnext = 0;
                tr->state = TRAIN_CIRCUITING;
                for (i = 0; i < tr->path->hops; i++) {
                    track_softreserve(&tr->res, tr->path->edges[i]);
                    track_disable(&tr->res, tr->path->edges[i]->reverse);
                    j = tr->respath.earliest;
                    while (j != tr->respath.next) {
                        if (tr->respath.edges[j] == tr->path->edges[i]) {
                            tr->respath.is_sub[j] = true;
                            break;
                        }
                        j++;
                        j %= ARRAY_SIZE(tr->respath.edges);
                    }
                }
            } else {
                trainsrv_stop(tr, STOP_AT_DEST);
            }
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

        assert(tr->pctrl.path_state != PATH_ON
            || TRACK_EDGE_DATA(tr->track,
                tr->pctrl.centre.edge,
                tr->path->edge_ix) >= 0);
        sensdist_um = trainsrv_distance_path(tr, &tr->pctrl, sens_pt);
        sensdist_um -= TRAIN_LENGTH_UM / 2;
        if (tr->pctrl.reversed)
            sensdist_um += TRAIN_BACK_OFFS_UM;
        else
            sensdist_um += TRAIN_FRONT_OFFS_UM;

        travel_um = trainsrv_predict_dist_um(
            tr,
            tr->pctrl.est_time + tr->sensor_window_ticks);

        if (sensdist_um > travel_um) {
            break;
        }

        trainsrv_expect_sensor(tr, sens);
    }
}

static void
trainsrv_pctrl_switch_turnouts(struct train *tr, bool switch_all)
{
    while ((unsigned)tr->path_swnext <= tr->path->hops) {
        struct track_pt sw_pt;
        track_node_t branch;
        bool curved;

        if ((unsigned)tr->path_swnext < tr->path->hops) {
            branch = tr->path->edges[tr->path_swnext]->src;
        } else {
            branch = tr->path->edges[tr->path->hops - 1]->dest;
            if (branch->type != TRACK_NODE_MERGE) {
                trainsrv_swnext_advance(tr);
                break;
            }
        }
        if (branch->type == TRACK_NODE_BRANCH) {
            sw_pt.edge   = tr->path->edges[tr->path_swnext];
            sw_pt.pos_um = 1000 * sw_pt.edge->len_mm;
        } else {
            //assert(branch->type == TRACK_NODE_MERGE);
            if (branch->type != TRACK_NODE_MERGE) {
                struct clkctx clock;
                clkctx_init(&clock);
                Delay(&clock, 100);
                panic("%s:%d: failed assertion: branch->Type == TRACK_NODE_MERGE", __FILE__, __LINE__);
            }
            sw_pt.edge   = tr->path->edges[tr->path_swnext - 1];
            sw_pt.pos_um = TRAIN_MERGE_OFFSET_UM;
        }

        int dist_um;
        assert(tr->pctrl.path_state != PATH_ON
            || TRACK_EDGE_DATA(tr->track,
                tr->pctrl.centre.edge,
                tr->path->edge_ix) >= 0);
        dist_um  = trainsrv_distance_path(tr, &tr->pctrl, sw_pt);
        dist_um -= TRAIN_LENGTH_UM / 2;

        if (switch_all)
            dist_um -= tr->pctrl.stop_um;
        else
            dist_um -= trainsrv_predict_dist_um(tr,
                tr->pctrl.est_time + tr->sensor_window_ticks);

        if (dist_um > 0)
            break;

        if (branch->type == TRACK_NODE_BRANCH) {
            curved = sw_pt.edge == &sw_pt.edge->src->edge[TRACK_EDGE_CURVED];
        } else {
            curved = sw_pt.edge
                == sw_pt.edge->reverse->src->edge[TRACK_EDGE_CURVED].reverse;
        }

        dbglog(&tr->dbglog, "train%d setting %d to %s",
            tr->train_id,
            branch->num,
            curved ? "curved" : "straight");
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
    if (tr->state == TRAIN_PARKED)
        est.dest.edge = NULL;
    else
        est.dest      = tr->final_dest;
    est.respath  = tr->respath;

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
train_set_speed(struct trainctx *ctx, uint8_t speed)
{
    struct trainmsg msg;
    int rplylen;
    msg.type  = TRAINMSG_SET_SPEED;
    msg.speed = speed;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

void
train_set_rev_penalty_mm(struct trainctx *ctx, int penalty_mm)
{
    struct trainmsg msg;
    int rplylen;
    msg.type           = TRAINMSG_SET_REV_PENALTY;
    msg.rev_penalty_mm = penalty_mm;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

/* Set desired cruising speed for the train */
void
train_set_rev_slack_mm(struct trainctx *ctx, int slack_mm)
{
    struct trainmsg msg;
    int rplylen;
    msg.type         = TRAINMSG_SET_REV_SLACK;
    msg.rev_slack_mm = slack_mm;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

/* Set desired cruising speed for the train */
void
train_set_init_rev_ok(struct trainctx *ctx, bool ok)
{
    struct trainmsg msg;
    int rplylen;
    msg.type        = TRAINMSG_SET_INIT_REV_OK;
    msg.init_rev_ok = ok;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

/* Set desired cruising speed for the train */
void
train_set_rev_ok(struct trainctx *ctx, bool ok)
{
    struct trainmsg msg;
    int rplylen;
    msg.type   = TRAINMSG_SET_REV_OK;
    msg.rev_ok = ok;
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

void
train_wander(struct trainctx *ctx)
{
    struct trainmsg msg;
    int rplylen;
    msg.type = TRAINMSG_WANDER;
    rplylen = Send(ctx->trainsrv_tid, &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

void
train_circuit(struct trainctx *ctx)
{
    struct trainmsg msg;
    int rplylen;
    msg.type = TRAINMSG_CIRCUIT;
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
