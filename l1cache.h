#ifdef L1CACHE_H
#error "double-included l1cache.h"
#endif

#define L1CACHE_H

/* Enable the L1 instruction and data caches. */
void l1cache_enable(void);

/* Disable the L1 instruction and data caches. */
void l1cache_disable(void);
