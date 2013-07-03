#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "static_assert.h"
#include "sensor.h"
#include "track_graph.h"
#include "calib.h"

const struct calib calib_initial[TRAINS_MAX][TRACK_COUNT] = {
    { /* train 0 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 1 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 2 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 3 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 4 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 5 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 6 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 7 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 8 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 9 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 10 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 11 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 12 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 13 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 14 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 15 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 16 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 17 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 18 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 19 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 20 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 21 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 22 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 23 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 24 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 25 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 26 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 27 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 28 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 29 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 30 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 31 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 32 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 33 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 34 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 35 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 36 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 37 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 38 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 39 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 40 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 41 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 42 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 43 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 44 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 45 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 46 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 47 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 48 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 49 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 50 */
        { /* track A */
            .vel_mmps = {
                0, 0, 0, 0, 0, 0, 0, 0, 430, 477, 523, 571, 616, 0, 0
            },
            .stop_dist = {
                0, 0, 0, 0, 0, 0, 0, 0, 450, 515, 580, 665, 720, 0, 0
            }
        },
        { /* track B */
            .vel_mmps = {
                0, 0, 0, 0, 0, 0, 0, 0, 429, 474, 527, 568, 618, 0, 0
            },
            .stop_dist = {
                0, 0, 0, 0, 0, 0, 0, 0, 450, 510, 590, 645, 705, 0, 0
            }
        }
    },
    { /* train 51 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 52 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 53 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 54 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 55 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 56 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 57 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 58 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 59 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 60 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 61 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 62 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 63 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 64 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 65 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 66 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 67 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 68 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 69 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 70 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 71 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 72 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 73 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 74 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 75 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 76 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 77 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 78 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 79 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 80 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 81 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 82 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 83 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 84 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 85 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 86 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 87 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 88 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 89 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 90 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 91 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 92 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 93 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 94 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 95 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 96 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 97 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 98 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 99 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 100 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 101 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 102 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 103 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 104 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 105 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 106 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 107 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 108 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 109 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 110 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 111 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 112 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 113 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 114 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 115 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 116 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 117 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 118 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 119 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 120 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 121 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 122 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 123 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 124 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 125 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 126 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 127 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 128 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 129 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 130 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 131 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 132 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 133 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 134 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 135 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 136 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 137 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 138 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 139 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 140 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 141 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 142 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 143 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 144 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 145 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 146 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 147 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 148 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 149 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 150 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 151 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 152 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 153 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 154 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 155 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 156 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 157 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 158 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 159 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 160 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 161 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 162 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 163 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 164 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 165 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 166 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 167 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 168 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 169 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 170 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 171 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 172 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 173 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 174 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 175 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 176 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 177 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 178 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 179 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 180 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 181 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 182 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 183 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 184 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 185 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 186 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 187 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 188 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 189 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 190 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 191 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 192 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 193 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 194 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 195 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 196 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 197 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 198 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 199 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 200 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 201 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 202 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 203 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 204 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 205 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 206 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 207 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 208 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 209 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 210 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 211 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 212 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 213 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 214 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 215 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 216 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 217 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 218 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 219 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 220 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 221 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 222 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 223 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 224 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 225 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 226 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 227 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 228 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 229 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 230 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 231 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 232 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 233 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 234 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 235 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 236 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 237 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 238 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 239 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 240 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 241 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 242 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 243 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 244 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 245 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 246 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 247 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 248 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 249 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 250 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 251 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 252 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 253 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 254 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
    { /* train 255 */
        { .vel_mmps = {0}, .stop_dist = {0} },
        { .vel_mmps = {0}, .stop_dist = {0} }
    },
};


