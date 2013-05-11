/* #include "bwio.h" */
/* #include "ts7200.h" */

#include "util.h"
#include "serial.h"
#include "clock.h"
#include "ringbuf.h"

#include "bwio.h"  /* bwprintf */

#define CLOCK_Hz 10

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

    /* main loop */
    for (;;) {
        char c;
        if (p_trygetc(P_TTY, &c))
            break;

        if (clock_update(&clock)) {
            if (rbuf_putc(&out, '.') != 0) {
                bwprintf(COM2, "failed to rbuf_putc\n");
                return 1;
            }
        }

        if (rbuf_peekc(&out, &c)) {
            if (p_tryputc(P_TTY, c))
                rbuf_getc(&out, &c);
        }
    }

    return 0;
}
