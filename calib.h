#ifdef CALIB_H
#error "double-included calib.h"
#endif

#define CALIB_H

CONFIG_H;
TRACK_GRAPH_H;

struct calib {
    int vel_umpt[16];  /* cruising velocity at each speed */
    int stop_um[16];   /* multiplier for current velocity at each speed */
};

extern const struct calib calib_initial[TRAINS_MAX][TRACK_COUNT];
