#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "xmath.h"
#include "poly.h"
#include "u_tid.h"
#include "static_assert.h"
#include "sensor.h"
#include "calib_srv.h"

#include "array_size.h"
#include "xassert.h"
#include "xmemcpy.h"
#include "track_graph.h"
#include "switch_srv.h"
#include "track_srv.h"
#include "track_pt.h"

#include "u_syscall.h"
#include "ns.h"
#include "tcmux_srv.h"
#include "sensor_srv.h"
#include "tracksel_srv.h"
#include "dbglog_srv.h"

#define VCALIB_ERR_THRESHOLD    20
#define VCALIB_SENSORS_SKIP     3

enum {
    CALMSG_VCALIB,
    CALMSG_STOPCALIB,
    CALMSG_GET,
};

struct calmsg {
    int          type;
    uint8_t      train_id;
    union {
        uint8_t  stopcalib_speed;
        struct {
            uint8_t minspeed, maxspeed;
        } vcalib;
    };
};

struct calsrv {
    struct tcmuxctx  tcmux;
    struct sensorctx sens;
    struct switchctx switches;
    struct trackctx  res;
    struct dbglogctx dbglog;
    track_graph_t    track;
    struct calib     all[TRAINS_MAX];
};

struct initcalib {
    uint8_t       train_id;
    struct calib  calib;
};

struct initalpha {
    uint8_t       train_id;
    track_graph_t track;
    float         alpha;
};

static const struct initcalib initcalib[];
static const unsigned int     n_initcalib;
static const struct initalpha initalpha[];
static const unsigned int     n_initalpha;

static void calibsrv_scale(struct calib *calib, float alpha);
static void calibsrv_vcalib(struct calsrv *cal, struct calmsg *msg);
static void calibsrv_stopcalib(struct calsrv *cal, struct calmsg *msg);
static track_node_t calibsrv_await(
    struct calsrv *cal, track_node_t sensor, int *when);

void
calibsrv_main(void)
{
    struct calsrv cal;
    struct calmsg msg;
    tid_t client;
    int rc, msglen;
    unsigned i;

    /* Initialize */
    cal.track = tracksel_ask();
    tcmuxctx_init(&cal.tcmux);
    sensorctx_init(&cal.sens);
    switchctx_init(&cal.switches);
    trackctx_init(&cal.res, 1);
    dbglogctx_init(&cal.dbglog);
    memset(cal.all, '\0', sizeof (cal.all));
    for (i = 0; i < n_initcalib; i++) {
        const struct initcalib *ic = &initcalib[i];
        cal.all[ic->train_id] = ic->calib;
    }

    for (i = 0; i < n_initalpha; i++) {
        const struct initalpha *ia = &initalpha[i];
        if (ia->track == cal.track)
            calibsrv_scale(&cal.all[ia->train_id], ia->alpha);
    }

    /* Register */
    rc = RegisterAs("calibsrv");
    assertv(rc, rc == 0);

    /* Event loop */
    for (;;) {
        /* Get message */
        msglen = Receive(&client, &msg, sizeof (msg));
        assertv(msglen, msglen == sizeof (msg));
        switch (msg.type) {
        case CALMSG_GET:
            rc = Reply(client, &cal.all[msg.train_id], sizeof (struct calib));
            assertv(rc, rc == 0);
            break;
        case CALMSG_VCALIB:
            calibsrv_vcalib(&cal, &msg);
            rc = Reply(client, NULL, 0);
            assertv(rc, rc == 0);
            break;
        case CALMSG_STOPCALIB:
            calibsrv_stopcalib(&cal, &msg);
            rc = Reply(client, NULL, 0);
            assertv(rc, rc == 0);
            break;
        default:
            panic("invalid calibration server message type %d", msg.type);
        }
    }
}

static track_node_t
calibsrv_await(struct calsrv *cal, track_node_t sensor, int *when)
{
    sensors_t sensors[SENSOR_MODULES];
    sensor1_t num;
    unsigned  i;
    int       rc;

    if (sensor == NULL) {
        for (i = 0; i < ARRAY_SIZE(sensors); i++)
            sensors[i] = (sensors_t)-1;
    } else {
        assert(sensor->type == TRACK_NODE_SENSOR);
        num = sensor->num;
        for (i = 0; i < ARRAY_SIZE(sensors); i++)
            sensors[i] = 0;
        sensors[sensor1_module(num)] = 1 << sensor1_sensor(num);
    }
    rc = sensor_wait(&cal->sens, sensors, -1, when);
    assertv(rc, rc == SENSOR_TRIPPED);

    num = sensors_scan(sensors);
    return num == SENSOR1_INVALID ? NULL : &cal->track->nodes[num];
}

static void
calibsrv_calibsetup(struct calsrv *cal, uint8_t train)
{
    struct track_routespec rspec;
    struct track_route     to_loop_route;
    struct track_path     *to_loop;
    struct track_pt        train_pos, calib_start;
    track_node_t           orignode;
    unsigned               i;
    int                    rc;

    /* Locate the train. */
    tcmux_train_speed(&cal->tcmux, train, TRAIN_CRAWLSPEED);
    orignode = calibsrv_await(cal, NULL, NULL);
    tcmux_train_speed(&cal->tcmux, train, 0);

    /* Move train into position for calibration */
    track_pt_from_node(orignode, &train_pos);
    track_pt_from_node(cal->track->calib_sensors[0], &calib_start);

    track_routespec_init(&rspec);
    rspec.track        = cal->track;
    rspec.switches     = &cal->switches;
    rspec.res          = &cal->res;
    rspec.train_id     = train;
    rspec.src_centre   = train_pos;
    rspec.err_um       = 200000; /* FIXME hardcoded 20cm */
    rspec.rev_penalty_mm=0;
    rspec.rev_slack_mm = 0;
    rspec.init_rev_ok  = false;
    rspec.rev_ok       = false;
    rspec.train_len_um = 215000; /* FIXME hardcoded 21.5cm */
    rspec.dest         = calib_start;
    rspec.dest_unidir  = true;   /* must arrive in requested direction */

    rc = track_routefind(&rspec, &to_loop_route);
    if (rc != 0)
        return; /* calibration failed */

    assert(to_loop_route.n_paths == 1);
    to_loop = &to_loop_route.paths[0];

    /* Set all switches on path to destination */
    for (i = 0; i < to_loop->hops; i++) {
        const struct track_node *branch;
        const struct track_edge *edge;
        bool curved;
        edge   = to_loop->edges[i];
        branch = edge->src;
        if (branch->type != TRACK_NODE_BRANCH)
            continue;
        curved = edge == &branch->edge[TRACK_EDGE_CURVED];
        tcmux_switch_curve(&cal->tcmux, branch->num, curved);
    }

    /* Send train to destination. */
    tcmux_train_speed(&cal->tcmux, train, TRAIN_DEFSPEED);
    calibsrv_await(cal, cal->track->calib_sensors[0], NULL);
    tcmux_train_speed(&cal->tcmux, train, 0);

    /* Switch all switches for calibration loop. */
    for (i = 0; i < (unsigned)cal->track->n_calib_switches - 1; i++) {
        tcmux_switch_curve(
            &cal->tcmux,
            cal->track->calib_switches[i]->num,
            cal->track->calib_curved[i]);
    }

    tcmux_switch_curve_sync(
        &cal->tcmux,
        cal->track->calib_switches[i]->num,
        cal->track->calib_curved[i]);
}

static void
calibsrv_scale(struct calib *calib, float alpha)
{
    unsigned i;
    for (i = 0; i < ARRAY_SIZE(calib->vel_umpt); i++)
        calib->vel_umpt[i] = (int)(alpha * (float)calib->vel_umpt[i]);
    for (i = 0; i <= (unsigned)calib->accel.deg; i++)
        calib->accel.a[i] *= alpha;
    for (i = 0; i <= (unsigned)calib->stop_um.deg; i++)
        calib->stop.a[i] *= alpha;
}

static void
calibsrv_vcalib(struct calsrv *cal, struct calmsg *msg)
{
    unsigned          i;
    int               exclude_um, lap_um, laps;
    int               vel_umpt[16];
    float             alpha; /* linear adjustment factor */

    uint8_t train    = msg->train_id;
    uint8_t minspeed = msg->vcalib.minspeed;
    uint8_t maxspeed = msg->vcalib.maxspeed;
    uint8_t speed;

    /* Cap speeds to ensure they're valid. */
    if (maxspeed > 14)
        maxspeed = 14;
    if (minspeed < 1)
        minspeed = 1;

    /* Get train/track into position. */
    calibsrv_calibsetup(cal, train);

    /* Figure out total calibration run distance. */
    lap_um = 0;
    for (i = 0; i < (unsigned)cal->track->n_calib_sensors; i++)
        lap_um += 1000 * cal->track->calib_mm[i];

    exclude_um = 0;
    for (i = 0; i < VCALIB_SENSORS_SKIP; i++)
        exclude_um += 1000 * cal->track->calib_mm[i + 1];

    /* Calibrate the speeds! */
    for (speed = minspeed; speed <= maxspeed; speed++) {
        int start_time, end_time, travel_um, v, v_diff;

        tcmux_train_speed(&cal->tcmux, train, speed);
        calibsrv_await(cal,
            cal->track->calib_sensors[VCALIB_SENSORS_SKIP],
            &start_time);

        laps = 0;
        travel_um = -exclude_um;
        v = -9999;
        do {
            calibsrv_await(cal, cal->track->calib_sensors[0], &end_time);
            laps++;
            travel_um += lap_um;
            v_diff = v;
            v = travel_um / (end_time - start_time);
            v_diff -= v;
            dbglog(&cal->dbglog, "train%d speed %d v_diff=%d", train, speed, v_diff);
            if (v_diff < 0)
                v_diff = -v_diff;
        } while (v_diff > VCALIB_ERR_THRESHOLD && laps < 3);

        vel_umpt[speed] = v;
        dbglog(&cal->dbglog, "train%d speed %d = %d um/tick", train, speed, v);
    }

    tcmux_train_speed(&cal->tcmux, train, 0);

    /* Determine scaling factor */
    int ahi, alo;
    alpha = 1.f;
    for (speed = minspeed; speed <= maxspeed; speed++)
        alpha *= (float)vel_umpt[speed] / (float)cal->all[train].vel_umpt[speed];

    alpha = nthroot(maxspeed - minspeed + 1, alpha, 4);

    alo  = (int)(alpha * 10000);
    ahi  = alo / 10000;
    alo %= 10000;
    dbglog(&cal->dbglog, "calibration alpha=%d.%04d", ahi, alo);

    /* Scale calibration values */
    calibsrv_scale(&cal->all[train], alpha);
    for (speed = minspeed; speed <= maxspeed; speed++)
        cal->all[train].vel_umpt[speed] = vel_umpt[speed];
}

static void
calibsrv_stopcalib(struct calsrv *cal, struct calmsg *msg)
{
    uint8_t train = msg->train_id;
    uint8_t speed = msg->stopcalib_speed;

    /* Cap speeds to ensure they're valid. */
    if (speed > 14)
        speed = 14;
    if (speed < 1)
        speed = 1;

    /* Get train/track into position. */
    calibsrv_calibsetup(cal, train);

    /* Calibrate the stopping distance! */
    tcmux_train_speed(&cal->tcmux, train, speed);
    calibsrv_await(cal, cal->track->calib_sensors[0], NULL);
    tcmux_train_speed(&cal->tcmux, train, 0);
}

void
calib_get(uint8_t train_id, struct calib *out)
{
    struct calmsg msg;
    int rplylen;
    msg.type     = CALMSG_GET;
    msg.train_id = train_id;
    rplylen = Send(WhoIs("calibsrv"), &msg, sizeof (msg), out, sizeof (*out));
    assertv(rplylen, rplylen == sizeof (*out));
}

void
calib_vcalib(uint8_t train_id, uint8_t minspeed, uint8_t maxspeed)
{
    struct calmsg msg;
    int rplylen;
    msg.type            = CALMSG_VCALIB;
    msg.train_id        = train_id;
    msg.vcalib.minspeed = minspeed;
    msg.vcalib.maxspeed = maxspeed;
    rplylen = Send(WhoIs("calibsrv"), &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

void
calib_stopcalib(uint8_t train_id, uint8_t speed)
{
    struct calmsg msg;
    int rplylen;
    msg.type            = CALMSG_STOPCALIB;
    msg.train_id        = train_id;
    msg.stopcalib_speed = speed;
    rplylen = Send(WhoIs("calibsrv"), &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

/* Initial calibration data. */
static const struct initcalib initcalib[] = {
#include "calib/initcalib.list"
};

static const unsigned int n_initcalib = ARRAY_SIZE(initcalib);

static const struct initalpha initalpha[] = {
    {
        .train_id = 45,
        .track    = &track_b,
        .alpha    = 0.9962f,
    },
};

static const unsigned int n_initalpha = ARRAY_SIZE(initalpha);
