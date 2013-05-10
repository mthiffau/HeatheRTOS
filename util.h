#ifdef UTIL_H
#error "double-included util.h"
#endif

#define UTIL_H

/*
 * Integer types
 */

typedef char           int8_t;
typedef unsigned char  uint8_t;
typedef int            int32_t;
typedef unsigned int   uint32_t;
typedef int            ssize_t;
typedef unsigned int   size_t;


/*
 * Boolean type.
 */

#define bool  _Bool
#define false 0
#define true  1


/*
 * Struct member offsets
 */

#define offsetof(st, m) ((size_t)(&((st*)0)->m))


/*
 * Static assert
 */

#define STATIC_ASSERT(name, x) \
    struct assert_##name { char a[(x) ? 1 : -1]; }


/*
 * Strong typedefs.
 */

#ifndef NDEBUG
/* Debug-mode compile. Use a struct to strongly typedef.
 * The name A should be a single identifier. */

#define NEWTYPE(A, T) \
    struct A { T __x; }; \
    static inline struct A mk##A(T t) { struct A r = { t }; return r; } \
    static inline T un##A(struct A a) { return a.__x; } \
    typedef struct A A

#define NTLIT(A, x) ((A) { .__x = (x) })

#else
/* Use the underlying type directly. */

#define NEWTYPE(A, T) \
    static inline T mk##A(T t) { return t; } \
    static inline T un##A(T t) { return t; } \
    typedef T A

#define NTLIT(A, x) (x)

#endif
