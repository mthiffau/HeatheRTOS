#include "cp15.h"

#include "cache.h"

/* Enable all caches */
void
cache_enable(void)
{
    CP15ICacheEnable();
    CP15DCacheEnable();
}

/* Disable all caches */
void
cache_disable(void)
{
    CP15DCacheDisable();
    CP15ICacheDisable();
}

/* Flush/Clean all caches */
void
cache_flush(void)
{
    CP15DCacheCleanFlush();
    CP15ICacheFlush();
}
