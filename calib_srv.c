#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "u_tid.h"
#include "static_assert.h"
#include "sensor.h"
#include "calib_srv.h"

#include "array_size.h"
#include "xassert.h"
#include "xmemcpy.h"
#include "track_graph.h"

#include "u_syscall.h"
#include "ns.h"
#include "tracksel_srv.h"

enum {
    CALMSG_UPDATE,
    CALMSG_GET,
};

struct calmsg {
    int          type;
    uint8_t      train_id;
    struct calib update_calib; /* CALMSG_UPDATE only */
};

struct calsrv {
    track_graph_t track;
    struct calib  all[TRAINS_MAX];
};

struct initcalib {
    uint8_t       train_id;
    track_graph_t track;
    struct calib  calib;
};

static const struct initcalib initcalib[];
static const unsigned int     n_initcalib;

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
    memset(cal.all, '\0', sizeof (cal.all));
    for (i = 0; i < n_initcalib; i++) {
        const struct initcalib *ic = &initcalib[i];
        if (ic->track == cal.track)
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
        case CALMSG_UPDATE:
            cal.all[msg.train_id] = msg.update_calib;
            rc = Reply(client, NULL, 0);
            assertv(rc, rc == 0);
            break;
        default:
            panic("invalid calibration server message type %d", msg.type);
        }
    }
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
calib_update(uint8_t train_id, const struct calib *calib)
{
    struct calmsg msg;
    int rplylen;
    msg.type         = CALMSG_UPDATE;
    msg.train_id     = train_id;
    msg.update_calib = *calib;
    rplylen = Send(WhoIs("calibsrv"), &msg, sizeof (msg), NULL, 0);
    assertv(rplylen, rplylen == 0);
}

/* Initial calibration data. */
static const struct initcalib initcalib[] = {
    {
        .train_id = 43,
        .track    = &track_a,
        .calib = {
            .vel_umpt = {
                0, 0, 0, 0, 0, 0, 0, 0, 3869, 4461, 4947, 5520, 5895, 0, 0
            },
            .stop_um = {
                0, 0, 0, 0, 0, 0, 0, 0, 581000, 664000, 748000, 823000, 919000, 0, 0
            }
        }
    },
    {
        .train_id = 43,
        .track    = &track_b,
        .calib = {
            .vel_umpt = {
                /* COPIED FROM TRACK A */
                0, 0, 0, 0, 0, 0, 0, 0, 3869, 4461, 4947, 5520, 5895, 0, 0
            },
            .stop_um = {
                /* COPIED FROM TRACK A */
                0, 0, 0, 0, 0, 0, 0, 0, 581000, 664000, 748000, 823000, 919000, 0, 0
            }
        }
    },
    {
        .train_id = 50,
        .track    = &track_a,
        .calib = {
            .vel_umpt = {
                0, 0, 0, 0, 0, 0, 0, 0, 3800, 4338, 4796, 5252, 5768, 0, 0
            },
            .stop_um = {
                0, 0, 0, 0, 0, 0, 0, 0, 450000, 515000, 580000, 665000, 720000, 0, 0
            }
        }
    },
    {
        .train_id = 50,
        .track    = &track_b,
        .calib = {
            .vel_umpt = {
                0, 0, 0, 0, 0, 0, 0, 0, 3801, 4338, 4796, 5252, 5768, 0, 0
            },
            .stop_um = {
                0, 0, 0, 0, 0, 0, 0, 0, 450000, 510000, 590000, 645000, 705000, 0, 0
            }
        }
    }
};

static const unsigned int n_initcalib = ARRAY_SIZE(initcalib);
