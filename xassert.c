#include "xbool.h"
#include "xassert.h"

#include "xdef.h"
#include "xint.h"
#include "xarg.h"
#include "cpumode.h"
#include "l1cache.h"
#include "ringbuf.h"

#include "bwio.h"

#include "u_tid.h"
#include "u_syscall.h"

static void uart_ctrl_delay(void);

void
panic(const char *fmt, ...)
{
    va_list args;
    if (cur_cpumode() == MODE_USR) {
        /* User-mode panic. Use Panic syscall */
        struct ringbuf buf;
        char mem[4096]; /* failing an assertion in rbuf_putc is BAD */
        rbuf_init(&buf, mem, sizeof (mem));
        va_start(args, fmt);
        rbuf_vprintf(&buf, fmt, args);
        va_end(args);
        rbuf_putc(&buf, '\0');
        Panic(mem);
    } else {
        /* Kernel mode panic. Use bwio and hang. */
        uart_ctrl_delay();
        bwsetspeed(COM2, 115200);
        uart_ctrl_delay();
        bwsetfifo(COM2, OFF);
        uart_ctrl_delay();
        va_start(args, fmt);
        bwformat(COM2, fmt, args);
        va_end(args);
        bwputc(COM2, '\n');
        for (;;) { }
    }
}

void
_assert(bool x, const char *file, int line, const char *expr)
{
    if (!__builtin_expect(x, 0))
        panic("%s:%d: failed assertion: %s", file, line, expr);
}

static void
uart_ctrl_delay(void)
{
    __asm__ (
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
    );
}
