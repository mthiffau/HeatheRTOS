#ifdef XDEF_H
#error "double-included xdef.h"
#endif

typedef long          ssize_t;
typedef unsigned long size_t;

#define offsetof(st, m) ((size_t)(&((st*)0)->m))
