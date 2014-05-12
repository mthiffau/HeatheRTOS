#ifndef XDEF_H
#define XDEF_H

/* Define NULL */
#define NULL ((void*)0)

/* Define size types */
typedef long          ssize_t;
typedef unsigned long size_t;

/* Macro to statically determine the offset of a field
   in a structure */
#define offsetof(st, m) ((size_t)(&((st*)0)->m))

#endif
