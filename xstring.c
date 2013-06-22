#include "xstring.h"

int
strcmp(const char *s1, const char *s2)
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

int
strlen(const char *s)
{
    int n = 0;
    while (*s++)
        n++;
    return n;
}

int
strnlen(const char *s, int maxlen)
{
    int i;
    for (i = 0; i < maxlen; i++) {
        if (s[i] == '\0')
            return i;
    }
    return maxlen;
}
