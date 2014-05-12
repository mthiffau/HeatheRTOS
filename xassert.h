#ifndef XASSERT_H
#define XASSERT_H

#include "xbool.h"

#ifdef NOASSERT
#define assertv(v,x)    ((void)(v))
#define assert(x)       assertv(0, x)
#else
#define assertv(v,x)    assert(x)
#define assert(x)       _assert((x), __FILE__, __LINE__, #x)
#endif

void panic(const char *fmt, ...)
    __attribute__((noreturn, format(printf, 1, 2)));
void _assert(bool x, const char *file, int line, const char *expr);

#endif
