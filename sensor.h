#ifdef SENSOR_H
#error "double-included sensor.h"
#endif

#define SENSOR_H

XINT_H;
STATIC_ASSERT_H;

/* Track sensor data size max */
#define SENSOR_MODULES      5
#define SENSORS_PER_MODULE  16
typedef uint16_t sensors_t;
typedef uint8_t  sensor1_t;

#define SENSOR1_INVALID     ((sensor1_t)-1)

STATIC_ASSERT(sensors_t_size, sizeof (sensors_t) * 8 >= SENSORS_PER_MODULE);
STATIC_ASSERT(sensor1_t_size,
    1LL << (8 * sizeof (sensor1_t)) >= SENSOR_MODULES * SENSORS_PER_MODULE + 1);

sensor1_t sensors_scan(sensors_t sensors[SENSOR_MODULES]);

static inline int
sensor1_module(sensor1_t s)
{
    return s / SENSORS_PER_MODULE;
}

static inline int
sensor1_sensor(sensor1_t s)
{
    return s % SENSORS_PER_MODULE;
}

static inline sensor1_t
mksensor1(int module, int sensor)
{
    return module * SENSORS_PER_MODULE + sensor;
}
