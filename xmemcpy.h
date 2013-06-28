#ifdef XMEMCPY_H
#error "double-included xmemcpy.h"
#endif

#define XMEMCPY_H

XDEF_H;

void *memcpy(void* dest, const void* src, size_t size);
void *memset(void *s, int c, size_t n);
