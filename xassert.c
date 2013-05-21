#include "xbool.h"
#include "xassert.h"

#include "xarg.h"
#include "bwio.h"

void
panic(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bwformat(COM2, fmt, args);
    va_end(args);
    bwputc(COM2, '\n');

    /* hang */
    for (;;) { }
}

void
_assert(bool x, const char *msg)
{
    if (!x)
        panic("%s\n", msg);
}
