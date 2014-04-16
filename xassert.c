#include "xbool.h"
#include "xassert.h"

#include "xdef.h"
#include "xint.h"
#include "xarg.h"
#include "cpumode.h"
#include "cache.h"
#include "ringbuf.h"

#include "bwio.h"

#include "u_tid.h"
#include "u_syscall.h"
#include "clock_srv.h"

void
panic(const char *fmt, ...)
{
    va_list args;
    if (cur_cpumode() == MODE_USR) {
        /* User-mode panic. Use Panic syscall */
        struct clkctx clock;
        struct ringbuf buf;
        char mem[4096]; /* failing an assertion in rbuf_putc is BAD */
        rbuf_init(&buf, mem, sizeof (mem));
        va_start(args, fmt);
        rbuf_vprintf(&buf, fmt, args);
        va_end(args);
        rbuf_putc(&buf, '\0');
        clkctx_init(&clock);
        if (clock.clksrv_tid != MyTid())
            Delay(&clock, 100); /* allow log messages to go out */
        Panic(mem);
    } else {
        /* Kernel mode panic. Use bwio and hang. */
	bwio_uart_setup();

        va_start(args, fmt);
        bwformat(fmt, args);
        va_end(args);

        bwputc('\n');
        for (;;) { }
    }
}

void
_assert(bool x, const char *file, int line, const char *expr)
{
    if (!__builtin_expect(x, 0))
        panic("%s:%d: failed assertion: %s", file, line, expr);
}

