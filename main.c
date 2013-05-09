#include "bwio.h"
#include "ts7200.h"

int main(int argc, char* argv[])
{
    int *pttyflags;
    int *ttydata;

    /* ignore arguments */
    (void)argc;
    (void)argv;

    /* disable FIFOs */
    /* bwsetfifo( COM1, OFF ); */
    bwsetfifo( COM2, OFF );

    /* get TTY register pointers */
    pttyflags = (int*)(UART2_BASE + UART_FLAG_OFFSET);
    ttydata   = (int*)(UART2_BASE + UART_DATA_OFFSET);

    /* Print whatever character is received, quit on q. */
    char c = '.';
    for (;;) {
        int ttyflags = *pttyflags;
        if (ttyflags & RXFF_MASK)
            c = *ttydata;

        if (c == 'q')
            break;

        if (!(ttyflags & TXFF_MASK))
            *ttydata = c;
    }

    return 0;
}
