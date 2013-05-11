/* #include "bwio.h" */
/* #include "ts7200.h" */

#include "util.h"
#include "serial.h"
#include "clock.h"
#include "ringbuf.h"

#include "bwio.h"  /* bwprintf */

#define CLOCK_Hz 10

#define ERASE_ALL   "\e[2J"
#define CURSOR_HOME "\e[;H"

int main(int argc, char* argv[])
{
    struct ringbuf out;
    struct clock   clock;
    int rc;

    /* ignore arguments */
    (void)argc;
    (void)argv;

    /* disable FIFO */
    p_enablefifo(P_TTY, false);

    /* initialize */
    rbuf_init(&out);
    if (clock_init(&clock, CLOCK_Hz) != 0) {
        bwprintf(COM2, "failed to initialize clock at %d Hz\n", CLOCK_Hz);
        return 1;
    }

    /* clear screen before sending any other output */
    rbuf_print(&out, ERASE_ALL);

    /* main loop */
    for (;;) {
        char c;
        if (p_trygetc(P_TTY, &c))
            break;

        if (rbuf_peekc(&out, &c)) {
            if (p_tryputc(P_TTY, c))
                rbuf_getc(&out, &c);
        }

        if (clock_update(&clock)) {
            uint32_t ticks, tenths, seconds, minutes;
            ticks   = clock_ticks(&clock);
            tenths  = ticks % 10;
            ticks   = ticks / 10;
            seconds = ticks % 60;
            ticks   = ticks / 60;
            minutes = ticks;
            rc = rbuf_printf(&out, CURSOR_HOME "%02u:%02u.%u",
                minutes, seconds, tenths);
            if (rc != 0) {
                bwprintf(COM2, "failed to rbuf_print\n");
                return 1;
            }
        }
    }

    return 0;
}
