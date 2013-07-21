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

/* Select priority queue implementation. */
#define PQ_RING
//#define PQ_HEAP

/* Trains */
#define TRAINS_MAX          256

#define TRAIN_CRAWLSPEED    2
#define TRAIN_MINSPEED      2
#define TRAIN_MAXSPEED      14
#define TRAIN_DEFSPEED      8

/* Application task priorities */
#define PRIORITY_NS         PRIORITY_MIN
#define PRIORITY_CLOCK      2
#define PRIORITY_SERIAL1    3
#define PRIORITY_SERIAL2    2
#define PRIORITY_TCMUX      6
#define PRIORITY_SENSOR     6
#define PRIORITY_SWITCH     6
#define PRIORITY_CALIB      PRIORITY_MIN
#define PRIORITY_TRAIN      8
#define PRIORITY_TRACKSEL   PRIORITY_MIN
#define PRIORITY_TRACK      6
#define PRIORITY_DBGLOG     1
#define PRIORITY_UI         13
