#ifdef ARRAY_SIZE_H
#error "double-included array_size.h"
#endif

#define ARRAY_SIZE_H

#define ARRAY_SIZE(a)   (sizeof (a) / sizeof (a[0]))
