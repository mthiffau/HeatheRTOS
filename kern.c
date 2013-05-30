#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "kern.h"

#include "ipc.h"

#include "xarg.h"
#include "xassert.h"
#include "xmemcpy.h"

#include "ctx_switch.h"
#include "intr.h"
#include "syscall.h"
#include "link.h"

#include "u_syscall.h"
#include "u_init.h"

#include "cpumode.h"
#include "bwio.h"

#define EXC_VEC_INSTR       0xe59ff018 /* ldr pc, [pc, #+0x18] */
#define EXC_VEC_SWI         ((unsigned int*)0x8)
#define EXC_VEC_FP(i)       (*((void**)((void*)(i) + 0x20)))

/* Default kernel parameters */
struct kparam def_kparam = {
    .init      = &u_init_main,
    .init_prio = U_INIT_PRIORITY
};

int
kern_main(struct kparam *kp)
{
    struct kern kern;

    /* Set up kernel state and create initial user task */
    kern_init(&kern);

    /* Run init - it's its own parent! */
    task_create(&kern, 0, kp->init_prio, kp->init);

    /* Main loop */
    while (!kern.shutdown) {
        struct task_desc *active;
        uint32_t          intr;

        active = task_schedule(&kern);
        if (active == NULL) {
            if (kern.ntasks == 0)
                break;
            else
                continue; /* FIXME this won't do once IRQs are in use */
        }

        intr = ctx_switch(active);
        kern_handle_intr(&kern, active, intr);
    }

    return 0;
}

void
kern_init(struct kern *kern)
{
    uint32_t i, mem_avail;

    /* Turn off UART FIFOs. */
    bwsetfifo(COM1, OFF);
    bwsetfifo(COM2, OFF);

    /* Install SWI handler. */
    *EXC_VEC_SWI = EXC_VEC_INSTR;
    EXC_VEC_FP(EXC_VEC_SWI) = &kern_entry_swi;

    /* Kernel hasn't been asked to shut down */
    kern->shutdown = false;

    /* 1MB reserved for kernel stack, move down to next 4k boundary */
    kern->stack_mem_top = (void*)(((uint32_t)&i - 0x100000) & 0xfffff000);

    /* Find largest number of 4k pages it is safe to allocate per task */
    mem_avail = kern->stack_mem_top - ProgramEnd;
    kern->stack_size = mem_avail / 0x1000 / MAX_TASKS * 0x1000;

    /* All tasks are free to begin with */
    kern->ntasks = 0;
    taskq_init(&kern->free_tasks);
    for (i = 0; i < MAX_TASKS; i++) {
        struct task_desc *td = &kern->tasks[i];
        td->state_prio = TASK_STATE_FREE; /* prio is arbitrary on init */
        td->tid_seq    = 0;
        td->next_ix    = TASK_IX_NOTINQUEUE;
        task_enqueue(kern, td, &kern->free_tasks);
    }

    /* All ready queues are empty */
    kern->rdy_queue_ne = 0;
    for (i = 0; i < N_PRIORITIES; i++)
        taskq_init(&kern->rdy_queues[i]);
}

void
kern_handle_intr(struct kern *kern, struct task_desc *active, uint32_t intr)
{
    switch (intr) {
    case INTR_SWI:
        kern_handle_swi(kern, active);
        break;
    default:
        panic("received unknown interrupt 0x%x\n", intr);
    }
}

void
kern_handle_swi(struct kern *kern, struct task_desc *active)
{
    uint32_t syscall = *((uint32_t*)active->regs->pc - 1) & 0x00ffffff;
    switch (syscall) {
    case SYSCALL_CREATE:
        active->regs->r0 = (uint32_t)task_create(
            kern,
            TASK_PTR2IX(kern, active),
            (int)active->regs->r0,
            (void(*)(void))active->regs->r1);
        task_ready(kern, active);
        break;
    case SYSCALL_MYTID:
        active->regs->r0 = (uint32_t)TASK_TID(kern, active);
        task_ready(kern, active);
        break;
    case SYSCALL_MYPARENTTID:
        active->regs->r0 = (uint32_t)TASK_TID(
            kern,
            TASK_IX2PTR(kern, active->parent_ix));
        task_ready(kern, active);
        break;
    case SYSCALL_PASS:
        task_ready(kern, active);
        break;
    case SYSCALL_EXIT:
        task_free(kern, active);
        break;
    case SYSCALL_SEND:
        ipc_send_start(kern, active);
        break;
    case SYSCALL_RECEIVE:
        ipc_receive_start(kern, active);
        break;
    case SYSCALL_REPLY:
        ipc_reply_start(kern, active);
        break;
    case SYSCALL_SHUTDOWN:
        kern->shutdown = true;
        break;
    default:
        panic("received unknown syscall 0x%x\n", syscall);
    }
}
