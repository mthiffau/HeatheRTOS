#include "config.h"
#include "xbool.h"
#include "xint.h"
#include "xdef.h"
#include "static_assert.h"
#include "u_tid.h"
#include "task.h"
#include "event.h"
#include "kern.h"

#include "ipc.h"

#include "xarg.h"
#include "xassert.h"
#include "xmemcpy.h"
#include "array_size.h"

#include "ctx_switch.h"
#include "intr_type.h"
#include "intr.h"
#include "timer.h"
#include "syscall.h"
#include "link.h"

#include "u_syscall.h"
#include "u_init.h"

#include "cpumode.h"
#include "bwio.h"

#define EXC_VEC_INSTR       0xe59ff018 /* ldr pc, [pc, #+0x18] */
#define EXC_VEC_SWI         ((unsigned int*)0x08)
#define EXC_VEC_IRQ         ((unsigned int*)0x18)
#define EXC_VEC_FP(i)       (*((void**)((void*)(i) + 0x20)))

static void kern_RegisterCleanup(struct kern *kern, struct task_desc *active);
static void kern_RegisterEvent(struct kern *kern, struct task_desc *active);
static void kern_AwaitEvent(struct kern *kern, struct task_desc *active);
static void kern_idle(void);

/* Default kernel parameters */
struct kparam def_kparam = {
    .init       = &u_init_main,
    .init_prio  = U_INIT_PRIORITY
};

int
kern_main(struct kparam *kp)
{
    struct kern kern;

    /* Set up kernel state and create initial user task */
    kern_init(&kern, kp);

    /* Main loop */
    while (!kern.shutdown && (kern.rdy_count > 1 || kern.evblk_count > 0)) {
        struct task_desc *active;
        uint32_t          intr;
        active = task_schedule(&kern);
        intr   = ctx_switch(active);
        kern_handle_intr(&kern, active, intr);
        assert(TASK_STATE(active) != TASK_STATE_ACTIVE);
    }

    kern_cleanup(&kern);
    return 0;
}

void
kern_init(struct kern *kern, struct kparam *kp)
{
    uint32_t i, mem_avail;
    tid_t tid;

    /* Install SWI and IRQ handlers. */
    *EXC_VEC_SWI = EXC_VEC_INSTR;
    *EXC_VEC_IRQ = EXC_VEC_INSTR;
    EXC_VEC_FP(EXC_VEC_SWI) = &kern_entry_swi;
    EXC_VEC_FP(EXC_VEC_IRQ) = &kern_entry_irq;

    /* Initialize event system */
    evt_init(&kern->eventab);

    /* 1MB reserved for kernel stack, move down to next 4k boundary */
    kern->stack_mem_top = (void*)(((uint32_t)&i - 0x100000) & 0xfffff000);

    /* Find largest number of 4k pages it is safe to allocate per task */
    mem_avail = kern->stack_mem_top - ProgramEnd;
    kern->stack_size = mem_avail / 0x1000 / MAX_TASKS * 0x1000;

    /* All tasks are free to begin with */
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

    /* Kernel hasn't been asked to shut down, and there aren't yet any
     * ready/event-blocked) tasks. */
    kern->shutdown    = false;
    kern->rdy_count   = 0;
    kern->evblk_count = 0;

    /* Start special tasks: idle and init. Each is its own parent. */
    tid = task_create(kern, 0, PRIORITY_IDLE, kern_idle);
    assertv(tid, tid == 0);
    tid = task_create(kern, 1, kp->init_prio, kp->init);
    assertv(tid, tid == 1);
}

void
kern_handle_intr(struct kern *kern, struct task_desc *active, uint32_t intr)
{
    switch (intr) {
    case INTR_SWI:
        kern_handle_swi(kern, active);
        break;
    case INTR_IRQ:
        kern_handle_irq(kern, active);
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
        if(active->cleanup != NULL)
            active->cleanup();
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
    case SYSCALL_REGISTERCLEANUP:
        kern_RegisterCleanup(kern, active);
        break;
    case SYSCALL_REGISTEREVENT:
        kern_RegisterEvent(kern, active);
        break;
    case SYSCALL_AWAITEVENT:
        kern_AwaitEvent(kern, active);
        break;
    case SYSCALL_SHUTDOWN:
        kern->shutdown = true;
        task_free(kern, active); /* must move out of ACTIVE state */
        break;
    case SYSCALL_PANIC:
        panic("%s", (const char*)active->regs->r0);
    default:
        panic("received unknown syscall 0x%x\n", syscall);
    }
}

void
kern_handle_irq(struct kern *kern, struct task_desc *active)
{
    int irq;
    struct event *evt;
    struct task_desc *wake;
    int rc, cb_rc;

    /* Interrupted task as always ready */
    task_ready(kern, active);

    /* Find the current event. */
    irq = evt_cur();
    evt = &kern->eventab.events[irq];
    assert(evt->tid >= 0);

    /* Run the associated callback. */
    cb_rc = evt->cb(evt->ptr, evt->size);
    assert(cb_rc >= -1);
    if (cb_rc == -1)
        return; /* callback says to ignore this interrupt */

    /* Look up the event-blocked task */
    rc = get_task(kern, evt->tid, &wake);
    assertv(rc, rc == GET_TASK_SUCCESS);
    assert(TASK_STATE(wake) == TASK_STATE_EVENT_BLOCKED);
    kern->evblk_count--;

    /* Ignore this interrupt until we get another AwaitEvent() for it */
    evt_disable(&kern->eventab, irq);

    /* Return from AwaitEvent() with the result of the callback */
    wake->regs->r0 = cb_rc;
    task_ready(kern, wake);
}

void
kern_cleanup(struct kern *kern)
{
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(kern->tasks); i++) {
        struct task_desc *td = &kern->tasks[i];
        if (TASK_STATE(td) != TASK_STATE_FREE && td->cleanup != NULL)
            td->cleanup();
    }
    evt_cleanup();
}

static void
kern_RegisterCleanup(struct kern *kern, struct task_desc *active)
{
    active->cleanup = (void (*)(void))active->regs->r0;
    task_ready(kern, active);
}

static void
kern_RegisterEvent(struct kern *kern, struct task_desc *active)
{
    int rc, irq;
    if (active->irq >= 0) {
        active->regs->r0 = EVT_DBL_REG;
        task_ready(kern, active);
        return;
    }

    irq = (int)active->regs->r0;
    rc  = evt_register(
        &kern->eventab,
        TASK_TID(kern, active),
        irq,
        (int(*)(void*,size_t))active->regs->r1);

    active->regs->r0 = rc;
    if (rc == 0)
        active->irq = irq;

    task_ready(kern, active);
}

static void
kern_AwaitEvent(struct kern *kern, struct task_desc *active)
{
    if (active->irq < 0) {
        active->regs->r0 = -1;
        task_ready(kern, active);
    }

    TASK_SET_STATE(active, TASK_STATE_EVENT_BLOCKED);
    evt_enable(
        &kern->eventab,
        active->irq,
        (void*)active->regs->r0,
        (size_t)active->regs->r1);
    kern->evblk_count++;
}

static void
kern_idle(void)
{
    for (;;) {
        /* Do nothing */
    }
}
