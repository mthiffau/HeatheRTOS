/* #include "bwio.h" */
/* #include "ts7200.h" */

#include "util.h"
#include "serial.h"
#include "clock.h"
#include "ringbuf.h"

#include "bwio.h"  /* bwprintf */

#define CLOCK_Hz 10

#define ESC_ERASE_SCREEN "\e[2J"
#define ESC_CURSOR_HOME  "\e[;H"

int main(int argc, char* argv[])
{
    struct ringbuf out;
    struct clock   clock;

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
    rbuf_print(&out, ESC_ERASE_SCREEN);

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
            uint32_t ticks = clock_ticks(&clock);
            if (rbuf_printf(&out, ESC_CURSOR_HOME "%04u", ticks) != 0) {
                bwprintf(COM2, "failed to rbuf_print\n");
                return 1;
            }
        }
    }

    return 0;
}
