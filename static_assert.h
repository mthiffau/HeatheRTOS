#ifndef STATIC_ASSERT_H
#define STATIC_ASSERT_H

/* Macro which acts as a compile time assertion */
#if 4 < __GNUC__ || (__GNUC__ == 4 && 6 <= __GNUC_MINOR__)
#define STATIC_ASSERT(name, x) _Static_assert(x, #name)
#else
#define STATIC_ASSERT(name, x) typedef char static_assert_##name[(x) ? 1 : -1]
#endif

#endif
