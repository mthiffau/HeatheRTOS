#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
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

        cal->all[train].vel_umpt[speed] = v;
        dbglog(&cal->dbglog, "train%d speed %d = %d um/tick", train, speed, v);
    }

    tcmux_train_speed(&cal->tcmux, train, 0);
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
    {
        .train_id = 45,
        .calib = {
            /* 19:57 Wed 17 July 2013, track B */
            .vel_umpt = {
                0,
                0,
                706,
                1234,
                1721,
                2306,
                2854,
                3454,
                3820,
                4254,
                4615,
                5168,
                5612,
                5893,
                0
            },
            /* 19:57 Wed 17 July 2013, track B */
            .stop_um = {
                0,
                0,
                60000  - 3 * 706,
                139000 - 3 * 1234,
                206000 - 3 * 1721,
                267000 - 3 * 2306,
                358000 - 3 * 2854,
                445000 - 3 * 3454,
                492000 - 3 * 3820,
                540000 - 3 * 4254,
                627000 - 3 * 4615,
                699000 - 3 * 5168,
                773000 - 3 * 5612,
                846000 - 3 * 6014,
                0
            },
            .stop_ref = 3820 + 4254 + 4615 + 5168 + 5612,
            /* 10pm? Tue 16 July 2013, track B */
            .accel = {
                .deg = 4,
                .a   = {
                    1.9212548188229261e+002,
                    1.6563720815027833e+002,
                    5.9044859742372934e+000,
                    -2.0497816203332430e-002,
                    6.4817958742409077e-005
                }
            },
            .accel_ref   = 3814 + 4227 + 4608 + 5155 + 5620,
            .accel_delay = 100,
            .accel_cutoff= {
                0,
                0,
                0,
                60,
                122,
                148,
                201,
                223,
                233,
                241,
                262,
                273,
                296,
                299,
                -1
            },
        }
    },
    {
        /* 23:09 Mon 08 July 2013, track B */
        .train_id = 50,
        .calib = {
            .vel_umpt = {
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                3801,
                4318,
                4807,
                5265,
                5784,
                0,
                0
            },
            .stop_um = {
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                443000 - 3 * 3801,
                519000 - 3 * 4318,
                584000 - 3 * 4807,
                657000 - 3 * 5265,
                703000 - 3 * 5784,
                0,
                0
            },
            .stop_ref = 3801 + 4318 + 4807 + 5265 + 5784,
            /* TODO more measurements and accel info */
        }
    }
};

static const unsigned int n_initcalib = ARRAY_SIZE(initcalib);
