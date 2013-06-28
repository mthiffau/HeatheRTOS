#ifdef XASSERT_H
#error "double-included xassert.h"
#endif

#define XASSERT_H

XBOOL_H;

#ifdef NOASSERT
#define assertv(v,x)    ((void)(v))
#define assert(x)       assertv(0, x)
#else
#define assertv(v,x)    assert(x)
#define assert(x)       _assert((x), __FILE__, __LINE__, #x)
#endif

void panic(const char *fmt, ...) __attribute__((noreturn));
void _assert(bool x, const char *file, int line, const char *expr);
