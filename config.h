#ifndef CONFIG_H
#define CONFIG_H

/* Maxima pertaining to task descriptors */
#define PAGE_SIZE           4096 /* All user stacks will be a multiple of this */
#define MAX_TASKS           128  /* Must be <= 254 to make room for sentinels */
#define N_PRIORITIES        16   /* Number of priorities in the system */
#define PRIORITY_MAX         0   /* Smallest priority number */
#define PRIORITY_MIN        14   /* Lowest priority number a user task can have */
#define PRIORITY_IDLE       15   /* Priority of the IDLE task */

/* Select priority queue implementation. */
#define PQ_RING
//#define PQ_HEAP

/* Application task priorities */
#define PRIORITY_NS         2 /* Name server priority */
#define PRIORITY_CLOCK      2 /* Clock server priority */
#define PRIORITY_SERIAL     2 /* Serial server priority */
#define PRIORITY_BLINK      3 /* Blink server priority */

#endif
