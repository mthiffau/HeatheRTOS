#ifndef XSTRING_H
#define XSTRING_H

/* Compare two strings */
int strcmp(const char *s1, const char *s2);
/* Get the length of a string */
int strlen(const char *s);
/* Get the length of string, without exploding */
int strnlen(const char *s, int maxlen);

#endif
