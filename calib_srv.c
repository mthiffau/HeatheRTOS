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
    struct dbglogctx dbglog;
    track_graph_t    track;
    struct calib     all[TRAINS_MAX];
};

struct initcalib {
    uint8_t       train_id;
    struct calib  calib;
};

static const struct initcalib initcalib[];
static const unsigned int     n_initcalib;

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
    dbglogctx_init(&cal.dbglog);
    memset(cal.all, '\0', sizeof (cal.all));
    for (i = 0; i < n_initcalib; i++) {
        const struct initcalib *ic = &initcalib[i];
        cal.all[ic->train_id] = ic->calib;
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
    struct track_path to_loop;
    struct track_pt   train_pos, calib_start;
    track_node_t      orignode;
    unsigned          i;
    int               rc;

    /* Locate the train. */
    tcmux_train_speed(&cal->tcmux, train, TRAIN_CRAWLSPEED);
    orignode = calibsrv_await(cal, NULL, NULL);
    tcmux_train_speed(&cal->tcmux, train, 0);

    /* Move train into position for calibration */
    track_pt_from_node(orignode, &train_pos);
    track_pt_from_node(cal->track->calib_sensors[0], &calib_start);
    rc = track_pathfind(
        cal->track,
        &train_pos,
        1,
        &calib_start,
        1,
        &to_loop);

    if (rc != 0)
        return; /* calibration failed */

    /* Set all switches on path to destination */
    for (i = 0; i < to_loop.n_branches; i++) {
        const struct track_node *branch;
        const struct track_edge *edge;
        bool curved;
        int j;
        branch = to_loop.branches[i];
        j      = TRACK_NODE_DATA(cal->track, branch, to_loop.node_ix);
        assert(j >= 0);
        if ((unsigned)j == to_loop.hops)
            break;
        edge   = to_loop.edges[j];
        curved = edge == &branch->edge[TRACK_EDGE_CURVED];
        if (i < to_loop.n_branches - 1)
            tcmux_switch_curve(&cal->tcmux, branch->num, curved);
        else
            tcmux_switch_curve_sync(&cal->tcmux, branch->num, curved);
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
calibsrv_vcalib(struct calsrv *cal, struct calmsg *msg)
{
    unsigned          i;
    int               exclude_um, lap_um;
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

        travel_um = -exclude_um;
        v = -9999;
        do {
            calibsrv_await(cal, cal->track->calib_sensors[0], &end_time);
            travel_um += lap_um;
            v_diff = v;
            v = travel_um / (end_time - start_time);
            v_diff -= v;
            if (v_diff < 0)
                v_diff = -v_diff;
            dbglog(&cal->dbglog, "train%d speed %d v_diff=%d", train, speed, v_diff);
        } while (v_diff > VCALIB_ERR_THRESHOLD);

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
    for (i = 0; i < ARRAY_SIZE(cal->all[train].vel_umpt); i++) {
        int newvel;
        if (i >= minspeed && i <= maxspeed)
            newvel = vel_umpt[i];
        else
            newvel = (int)(alpha * (float)cal->all[train].vel_umpt[i]);
        cal->all[train].vel_umpt[i] = newvel;
    }

    for (i = 0; i <= (unsigned)cal->all[train].stop_um.deg; i++)
        cal->all[train].stop_um.a[i] *= alpha;

    for (i = 0; i <= (unsigned)cal->all[train].accel.deg; i++)
        cal->all[train].accel.a[i] *= alpha;
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
