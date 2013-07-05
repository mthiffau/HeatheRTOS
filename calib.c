#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "calib.h"

const struct calib calib_initial[TRAINS_MAX][TRACK_COUNT] = {
    { /* train 0 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 1 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 2 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 3 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 4 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 5 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 6 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 7 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 8 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 9 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 10 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 11 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 12 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 13 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 14 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 15 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 16 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 17 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 18 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 19 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 20 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 21 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 22 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 23 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 24 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 25 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 26 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 27 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 28 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 29 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 30 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 31 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 32 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 33 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 34 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 35 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 36 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 37 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 38 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 39 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 40 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 41 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 42 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 43 */
        { /* track A */
            .vel_umpt = {
                0, 0, 0, 0, 0, 0, 0, 0, 3869, 4461, 4947, 5520, 5895, 0, 0
            },
            .stop_um = {
                0, 0, 0, 0, 0, 0, 0, 0, 581000, 664000, 748000, 823000, 919000, 0, 0
            }
        },
        { /* track B */
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
    { /* train 44 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 45 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 46 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 47 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 48 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 49 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 50 */
        { /* track A */
            .vel_umpt = {
                0, 0, 0, 0, 0, 0, 0, 0, 3801, 4338, 4796, 5252, 5768, 0, 0
            },
            .stop_um = {
                0, 0, 0, 0, 0, 0, 0, 0, 450000, 515000, 580000, 665000, 720000, 0, 0
            }
        },
        { /* track B */
            .vel_umpt = {
                0, 0, 0, 0, 0, 0, 0, 0, 3801, 4338, 4796, 5252, 5768, 0, 0
            },
            .stop_um = {
                0, 0, 0, 0, 0, 0, 0, 0, 450000, 510000, 590000, 645000, 705000, 0, 0
            }
        }
    },
    { /* train 51 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 52 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 53 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 54 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 55 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 56 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 57 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 58 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 59 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 60 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 61 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 62 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 63 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 64 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 65 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 66 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 67 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 68 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 69 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 70 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 71 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 72 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 73 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 74 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 75 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 76 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 77 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 78 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 79 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 80 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 81 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 82 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 83 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 84 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 85 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 86 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 87 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 88 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 89 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 90 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 91 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 92 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 93 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 94 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 95 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 96 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 97 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 98 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 99 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 100 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 101 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 102 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 103 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 104 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 105 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 106 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 107 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 108 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 109 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 110 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 111 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 112 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 113 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 114 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 115 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 116 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 117 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 118 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 119 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 120 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 121 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 122 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 123 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 124 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 125 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 126 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 127 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 128 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 129 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 130 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 131 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 132 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 133 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 134 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 135 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 136 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 137 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 138 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 139 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 140 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 141 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 142 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 143 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 144 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 145 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 146 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 147 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 148 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 149 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 150 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 151 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 152 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 153 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 154 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 155 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 156 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 157 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 158 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 159 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 160 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 161 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 162 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 163 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 164 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 165 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 166 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 167 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 168 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 169 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 170 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 171 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 172 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 173 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 174 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 175 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 176 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 177 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 178 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 179 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 180 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 181 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 182 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 183 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 184 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 185 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 186 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 187 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 188 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 189 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 190 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 191 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 192 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 193 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 194 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 195 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 196 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 197 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 198 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 199 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 200 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 201 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 202 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 203 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 204 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 205 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 206 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 207 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 208 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 209 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 210 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 211 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 212 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 213 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 214 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 215 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 216 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 217 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 218 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 219 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 220 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 221 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 222 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 223 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 224 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 225 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 226 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 227 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 228 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 229 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 230 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 231 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 232 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 233 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 234 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 235 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 236 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 237 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 238 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 239 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 240 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 241 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 242 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 243 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 244 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 245 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 246 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 247 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 248 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 249 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 250 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 251 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 252 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 253 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 254 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
    { /* train 255 */
        { .vel_umpt = {0}, .stop_um = {0} },
        { .vel_umpt = {0}, .stop_um = {0} }
    },
};


