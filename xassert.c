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

