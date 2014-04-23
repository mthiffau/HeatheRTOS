#ifdef CACHE_H
#error "double-included cache.h"
#endif

#define CACHE_H

/* Enable the caches. */
void cache_enable(void);

/* Disable the caches. */
void cache_disable(void);

/* Flush the caches */
void cache_flush(void);
