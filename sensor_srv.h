#ifdef SENSOR_SRV
#error "double-included sersor_srv.h"
#endif

#define SENSOR_SRV

SENSOR_H;
U_TID_H;

#define SENSOR_AVG_DELAY_TICKS      3

/* Sensor server entry point. */
void sensrv_main(void);

/* Client functions */
struct sensorctx {
    tid_t sensrv_tid;
};

enum {
    SENSOR_TRIPPED = 0,
    SENSOR_TIMEOUT = -1,
};

void sensorctx_init(struct sensorctx *ctx);

/* Wait for one of the specified sensors to trip, or for the
 * specified timeout to expire, whichever happens first.
 * A negative timeout never expires.
 *
 * If returning due to one or more sensors tripping, returns
 * SENSOR_TRIPPED and fills the sensors array with the newly
 * tripped sensors. Otherwise, returns SENSOR_TIMEOUT.
 * The time of reply is put into when. */
int sensor_wait(
    struct sensorctx *ctx,
    sensors_t sensors[SENSOR_MODULES],
    int timeout,
    int *when);

/* Alert the sensor server of newly tripped sensors. */
void sensor_report(struct sensorctx *ctx, sensors_t sensors[SENSOR_MODULES]);
