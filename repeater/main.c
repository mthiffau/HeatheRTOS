#include "xbool.h"
#include "xint.h"
#include "xarg.h"
#include "bwio.h"
#include "l1cache.h"
#include "timer.h"
#include "ts7200.h"

int
main(int argc, char *argv[])
{
    volatile int * const data1 = (volatile int*)(UART1_BASE + UART_DATA_OFFSET);
    volatile int * const flag1 = (volatile int*)(UART1_BASE + UART_FLAG_OFFSET);
    volatile int * const data2 = (volatile int*)(UART2_BASE + UART_DATA_OFFSET);
    volatile int * const flag2 = (volatile int*)(UART2_BASE + UART_FLAG_OFFSET);
    char c1    = '\0';
    bool full1 = false;
    char c2    = '\0';
    bool full2 = '\0';
    int32_t max_loop_time = 0, last_loop_start = -1;
    bool    suppress = false;
    int     tildes = 0;

    (void)argc;
    (void)argv;

    bwsetfifo(COM1, OFF);
    bwsetfifo(COM2, OFF);

    bwputstr(COM2, "starting repeater\n");

    l1cache_enable();
    tmr40_reset();

    while (tildes < 4) {
        int32_t loop_start = tmr40_get();
        if (last_loop_start >= 0) {
            int32_t loop_time = loop_start - last_loop_start;
            if (loop_time > max_loop_time)
                max_loop_time = loop_time;
        }
        last_loop_start = loop_start;

        if (*flag1 & RXFF_MASK) {
            c2 = (char)(*data1 & 0xff);
            full2 = true;
            suppress = false;
        }
        if (*flag2 & RXFF_MASK) {
            c1 = (char)(*data2 & 0xff);
            full1 = true;
            if (c1 == '~')
                tildes++;
            else
                tildes = 0;
        }
        if (full1 && (*flag1 & TXFE_MASK)) {
            *data1 = c1;
            full1 = false;
        }
        if (*flag2 & TXFE_MASK) {
            if (full2) {
                *data2 = c2;
                full2 = false;
            } else if (!suppress) {
                *data2 = '\a';
                suppress = true;
            }
        }
    }

    bwprintf(COM2,
        "exiting repeater: longest loop took %d us\n",
        max_loop_time * 1000 / 983);
    return 0;
}
