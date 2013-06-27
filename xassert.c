#include "xbool.h"
#include "xassert.h"

#include "xdef.h"
#include "xint.h"
#include "xarg.h"
#include "cpumode.h"
#include "ringbuf.h"

#include "bwio.h"

#include "u_tid.h"
#include "u_syscall.h"

void
panic(const char *fmt, ...)
{
    va_list args;
    if (cur_cpumode() == MODE_USR) {
        /* User-mode panic. Use Panic syscall */
        struct ringbuf buf;
        char mem[512];
        rbuf_init(&buf, mem, sizeof (mem));
        va_start(args, fmt);
        rbuf_printf(&buf, fmt, args);
        va_end(args);
        Panic(mem);
    } else {
        /* Kernel mode panic. Use bwio and hang. */
        va_start(args, fmt);
        bwformat(COM2, fmt, args);
        va_end(args);
        bwputc(COM2, '\n');
        for (;;) { }
    }
}

void
_assert(bool x, const char *msg)
{
    if (!x)
        panic("%s\n", msg);
}
