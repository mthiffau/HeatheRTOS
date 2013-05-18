#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "cpumode.h"
#include "task.h"
#include "ctx_switch.h"
#include "bwio.h"
#include "ts7200.h"

#define EXC_VEC_INSTR       0xe59ff018 /* ldr pc, [pc, #+0x18] */
#define EXC_VEC_SWI         ((unsigned int*)0x8)
#define EXC_VEC_FP(i)       (*((void**)((void*)(i) + 0x20)))

void
task_inner(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        bwprintf(COM2, "%s inner %d\n", cpumode_name(cur_cpumode()), i);
        trigger_swi();
    }
}

void
task_main(void)
{
    int i = 0;
    bwputstr(COM2, "task_main start\n");
    for (;;) {
        char c = bwgetc(COM2);
        bwprintf(COM2, "%s main %d %c\n", cpumode_name(cur_cpumode()), i++, c);
        trigger_swi();
        task_inner();
    }
}

int
main()
{
    int i;
    struct task_desc curtask;

    *EXC_VEC_SWI = EXC_VEC_INSTR;
    EXC_VEC_FP(EXC_VEC_SWI) = &kern_entry_swi;
    bwsetfifo(COM2, OFF);

    /* Set up task */
    curtask.state = (struct task_state*)0x01000000 - 1;
    curtask.state->spsr = cpumode_bits(MODE_USR); /* all interrupts enabled */
    curtask.state->pc   = (uint32_t)&task_main;

    for (i = 0; i < 100; i++) {
        uint32_t ev = ctx_switch(&curtask);
        const char *mode = cpumode_name(cur_cpumode());
        bwprintf(COM2, "%s event = %d\n", mode, ev);
        bwprintf(COM2, "%s usrsp = %x\n", mode, curtask.state + 1);
    }

    return 0;
}
