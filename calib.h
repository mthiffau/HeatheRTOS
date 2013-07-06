#ifdef CALIB_H
#error "double-included calib.h"
#endif

#define CALIB_H

CONFIG_H;
TRACK_GRAPH_H;

struct calib {
    int vel_umpt[16];  /* cruising velocity at each speed */
    int stop_um[16];   /* stopping distance at each speed */
};

/* Calibration server! */

/* Returns NULL if no static initial calibration is known. */
const struct calib *initcalib_get(uint8_t train_id, uint8_t track_id);
