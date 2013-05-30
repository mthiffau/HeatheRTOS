#ifdef MEMCPY_H
#error "double-included memcpy.h"
#endif

#define MEMCPY_H

XDEF_H;

void* xmemcpy(void* dest, const void* src, size_t size);
