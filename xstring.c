/*******************************************************************************
    Copyright 2014 Matthew Thiffault

    This file is part of HeatheRTOS.

    HeatheRTOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    HeatheRTOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HeatheRTOS.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************/

#include "xstring.h"

/* Compare two strings */
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

/* Get the length of a string */
int
strlen(const char *s)
{
    int n = 0;
    while (*s++)
        n++;
    return n;
}

/* Get the bounded length of a string */
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
