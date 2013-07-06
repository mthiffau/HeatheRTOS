#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "calib.h"

#include "array_size.h"

#define TRACK_A     0
#define TRACK_B     1

struct initcalib {
    uint8_t      train_id;
    uint8_t      track_id;
    struct calib calib;
};

static const struct initcalib initcalib[];
static const unsigned int     n_initcalib;

const struct calib*
initcalib_get(uint8_t train_id, uint8_t track_id)
{
    unsigned i;
    for (i = 0; i < n_initcalib; i++) {
        const struct initcalib *ic = &initcalib[i];
        if (ic->train_id == train_id && ic->track_id == track_id)
            return &ic->calib;
    }
    return NULL;
}

static const struct initcalib initcalib[] = {
    {
        .train_id = 43,
        .track_id = TRACK_A,
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
        .track_id = TRACK_B,
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
        .track_id = TRACK_A,
        .calib = {
            .vel_umpt = {
                0, 0, 0, 0, 0, 0, 0, 0, 3801, 4338, 4796, 5252, 5768, 0, 0
            },
            .stop_um = {
                0, 0, 0, 0, 0, 0, 0, 0, 450000, 515000, 580000, 665000, 720000, 0, 0
            }
        }
    },
    {
        .train_id = 50,
        .track_id = TRACK_B,
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
