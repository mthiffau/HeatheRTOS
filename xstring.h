#ifdef XSTRING_H
#error "double-included xstring.h"
#endif

#define XSTRING_H

int strcmp(const char *s1, const char *s2);
int strnlen(const char *s, int maxlen);
