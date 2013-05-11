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

#if 4 < __GNUC__ || (__GNUC__ == 4 && 6 <= __GNUC_MINOR__)
#define STATIC_ASSERT(name, x) _Static_assert(x, #name)
#else
#define STATIC_ASSERT(name, x) typedef char static_assert_##name[(x) ? 1 : -1]
#endif

/*
 * Variadic arguments
 */

typedef char *va_list;

#define __va_argsiz(t) \
    (((sizeof(t) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))

#define va_start(ap, pN) \
    ((ap) = ((va_list) __builtin_next_arg(pN)))

#define va_end(ap) \
    ((void)0)

#define va_arg(ap, t) \
    (((ap) = (ap) + __va_argsiz(t)), *((t*) (void*) ((ap) - __va_argsiz(t))))


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
