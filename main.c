/* #include "bwio.h" */
/* #include "ts7200.h" */

#include "util.h"
#include "serial.h"

#include "bwio.h"  /* bwprintf */

int main(int argc, char* argv[])
{
    /* ignore arguments */
    (void)argc;
    (void)argv;

    /* disable FIFO */
    p_enablefifo(P_TTY, false);

    /* Print whatever character is received, quit on q. */
    char c = '.';
    for (;;) {
        bool got = p_trygetc(P_TTY, &c);
        if (c == 'q')
            break;
        else if (got)
            p_tryputc(P_TTY, c);
    }

    return 0;
}
