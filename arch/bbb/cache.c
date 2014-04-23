#include "cp15.h"

#include "cache.h"

void
cache_enable(void)
{
    CP15ICacheEnable();
    CP15DCacheEnable();
}

void
cache_disable(void)
{
    CP15DCacheDisable();
    CP15ICacheDisable();
}

void
cache_flush(void)
{
    CP15DCacheCleanFlush();
    CP15ICacheFlush();
}
