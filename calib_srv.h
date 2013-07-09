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

/* Do a velocity calibration run. */
void calib_vcalib(uint8_t train_id, uint8_t minspeed, uint8_t maxspeed);

/* Do a stopping distance calibration run. */
void calib_stopcalib(uint8_t train_id, uint8_t speed);
