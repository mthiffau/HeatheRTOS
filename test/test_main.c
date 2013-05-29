#include "xarg.h"
#include "bwio.h"

int
main(void)
{
    bwsetfifo(COM2, OFF);
    bwputstr(COM2, "hello test\n");
    return 0;
}
