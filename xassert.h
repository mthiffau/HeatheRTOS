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
#define assert(x)                                      \
    do {                                               \
        if (!__builtin_expect((x), 0))                 \
            panic("%s:%d: %s",                         \
                  __FILE__,                            \
                  __LINE__,                            \
                  "assertion (" #x ") failed!");       \
    } while (0)
#endif

void panic(const char *fmt, ...) __attribute__((noreturn));
