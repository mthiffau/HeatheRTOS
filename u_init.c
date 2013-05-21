#include "u_tid.h"
#include "u_init.h"
#include "u_syscall.h"

#include "xint.h"
#include "cpumode.h"

#include "xarg.h"
#include "bwio.h"

static void
inner(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        bwprintf(COM2, "%s inner %d\n", cpumode_name(cur_cpumode()), i);
        Pass();
    }
}

void
u_init_main(void)
{
    int i = 0;
    bwputstr(COM2, "u_init_main start\n");
    for (;;) {
        char c = bwgetc(COM2);
        bwprintf(COM2, "%s main %d %c\n", cpumode_name(cur_cpumode()), i++, c);
        Pass();
        inner();
    }
}
