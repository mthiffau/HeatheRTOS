#include "xint.h"
#include "xdef.h"
#include "xarg.h"
#include "static_assert.h"

#include "cpumode.h"
#include "task.h"
#include "ctx_switch.h"
#include "u_init.h"

#include "ts7200.h"
#include "bwio.h"

#define EXC_VEC_INSTR       0xe59ff018 /* ldr pc, [pc, #+0x18] */
#define EXC_VEC_SWI         ((unsigned int*)0x8)
#define EXC_VEC_FP(i)       (*((void**)((void*)(i) + 0x20)))

void
__attribute__((noreturn))
panic(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bwformat(COM2, fmt, args);
    va_end(args);

    /* hang */
    for (;;) { }
}

int
main()
{
    int i;
    struct task_desc curtask;

    bwsetfifo(COM2, OFF);

    *EXC_VEC_SWI = EXC_VEC_INSTR;
    EXC_VEC_FP(EXC_VEC_SWI) = &kern_entry_swi;

    /* Set up task */
    curtask.state = (struct task_state*)0x01000000 - 1;
    curtask.state->spsr = cpumode_bits(MODE_USR); /* all interrupts enabled */
    curtask.state->pc   = (uint32_t)&u_init_main;

    for (i = 0; i < 100; i++) {
        uint32_t ev = ctx_switch(&curtask);
        const char *mode = cpumode_name(cur_cpumode());
        bwprintf(COM2, "%s event = %d\n", mode, ev);
        bwprintf(COM2, "%s usrsp = %x\n", mode, curtask.state + 1);
    }

    return 0;
}
