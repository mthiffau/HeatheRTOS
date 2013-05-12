#include "xstring.h"

int strcmp(const char *s1, const char *s2)
{
    signed char c1, c2;
    for (;;) {
        c1 = (signed char)*s1++;
        c2 = (signed char)*s2++;
        if (c1 != c2)
            return (int)c1 - (int)c2;
        else if (c1 == '\0')
            return 0;
    }
}
