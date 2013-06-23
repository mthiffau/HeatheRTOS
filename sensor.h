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

STATIC_ASSERT(sensors_t_size, sizeof (sensors_t) * 8 >= SENSORS_PER_MODULE);
