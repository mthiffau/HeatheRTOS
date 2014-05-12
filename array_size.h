#ifndef ARRAY_SIZE_H
#define ARRAY_SIZE_H

/* Give the size of a statically defined array */
#define ARRAY_SIZE(a)   (sizeof (a) / sizeof (a[0]))

#endif
