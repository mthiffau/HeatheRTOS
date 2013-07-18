#ifdef CALIB_SRV_H
#error "double-included calib_srv.h"
#endif

#define CALIB_SRV_H

CONFIG_H;
XBOOL_H;
XINT_H;
POLY_H;
U_TID_H;

struct calib {
    int         vel_umpt[16];     /* cruising velocity at each speed */

    /* Stopping distances */
    struct poly stop_um;          /* stopping distance given velocity */
    int         stop_ref;         /* reference value for stopping distances */

    /* Acceleration data. */
    struct poly accel;
    int         accel_delay;      /* delay in ticks before train moves */
    int         accel_cutoff[16]; /* cutoff time in ticks for each speed */
    int         accel_ref;        /* reference value for acceleration curve.
                                     = sum of velocities at speeds 8-12. */
};

/* calibration server entry point. */
void calibsrv_main(void);

/* Get calibration data for a train. */
void calib_get(uint8_t train_id, struct calib *out);

/* Do a velocity calibration run. */
void calib_vcalib(uint8_t train_id, uint8_t minspeed, uint8_t maxspeed);

/* Do a stopping distance calibration run. */
void calib_stopcalib(uint8_t train_id, uint8_t speed);
