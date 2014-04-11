#ifdef L1CACHE_H
#error "double-included l1cache.h"
#endif

#define L1CACHE_H

/* Enable the L1 instruction and data caches. */
void cache_enable(void);

/* Disable the L1 instruction and data caches. */
void cache_disable(void);

/* Get the value of the cache type register. */
uint32_t cache_typereg(void);
