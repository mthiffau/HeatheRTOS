#include "xint.h"
#include "static_assert.h"
#include "sensor.h"

#include "bithack.h"

sensor1_t
sensors_scan(sensors_t sensors[SENSOR_MODULES])
{
    int i;
    for (i = 0; i < SENSOR_MODULES; i++) {
        if (sensors[i] != 0)
            return mksensor1(i, ctz16(sensors[i]));
    }
    return SENSOR1_INVALID;
}
