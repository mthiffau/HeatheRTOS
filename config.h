#ifdef CONFIG_H
#error "double-included config.h"
#endif

#define CONFIG_H

/* Maxima pertaining to task descriptors */
#define MAX_TASKS           128  /* must be <= 254 to make room for sentinels */
#define N_PRIORITIES        16
#define PRIORITY_MAX         0
#define PRIORITY_MIN        14
#define PRIORITY_IDLE       15
#define IRQ_PRIORITIES      16

/* Select priority queue implementation. */
#define PQ_RING
/* #define PQ_HEAP */

/* Track sensor data size max */
#define SENSOR_BYTES        10
