#ifdef CALIB_SRV_H
#error "double-included calib_srv.h"
#endif

#define CALIB_SRV_H

CONFIG_H;
XBOOL_H;
XINT_H;
U_TID_H;

struct calib {
    int vel_umpt[16];  /* cruising velocity at each speed */
    int stop_um[16];   /* stopping distance at each speed */
};

/* calibration server entry point. */
void calibsrv_main(void);

/* Get calibration data for a train. */
void calib_get(uint8_t train_id, struct calib *out);

/* Update calibration data. */
void calib_update(uint8_t train_id, const struct calib *calib);
