#ifdef CONFIG_H
#error "double-included config.h"
#endif

#define CONFIG_H

/* Maxima pertaining to task descriptors */
#define MAX_TASKS           128  /* must be <= 254 to make room for sentinels */
#define N_PRIORITIES        16
#define IRQ_PRIORITIES      16
