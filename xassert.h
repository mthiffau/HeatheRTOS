#ifdef XASSERT_H
#error "double-included xassert.h"
#endif

#define XASSERT_H

XBOOL_H;

#ifdef NOASSERT
#define assert(x) ((void)0)
#else
#define assert(x)                                      \
    do {                                               \
        if (!(x))                                      \
            panic("%s:%d: %s",                         \
                  __FILE__,                            \
                  __LINE__,                            \
                  "assertion (" #x ") failed!");       \
    } while (0)
#endif

void panic(const char *fmt, ...) __attribute__((noreturn));
