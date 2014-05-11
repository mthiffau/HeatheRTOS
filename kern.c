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

#include "exc_vec.h"

#ifdef HARD_FLOAT
#include "vfp.h"
#endif

static void kern_top_pct(uint32_t total, uint32_t amt);
static void kern_top(struct kern *kern, uint32_t total_time);
static void kern_RegisterCleanup(struct kern *kern, struct task_desc *active);
static void kern_RegisterEvent(struct kern *kern, struct task_desc *active);
static void kern_AwaitEvent(struct kern *kern, struct task_desc *active);
static void kern_idle(void);

/* Default kernel parameters */
struct kparam def_kparam = {
    .init       = &u_init_main,
    .init_prio  = U_INIT_PRIORITY,
    .show_top   = true
};

int
kern_main(struct kparam *kp)
{
    struct kern kern;
    uint32_t start_time, end_time, time;

    /* Set up kernel state and create initial user task */
    kern_init(&kern, kp);

    bwprintf("\n\rKernel initialized:\n\r");
    bwprintf("Kernel Stack -- Bottom: %x Top: %x Size: %d bytes\n\r", 
	     (unsigned int)KernStackBottom, 
	     (unsigned int)KernStackTop, 
	     (unsigned int)(KernStackBottom - KernStackTop));
    bwprintf("User Stacks -- Bottom: %x Top: %x Size: %d bytes\n\r",
	     (unsigned int)UserStacksEnd,
	     (unsigned int)UserStacksStart,
	     (unsigned int)kern.user_stack_size);

    /* Main loop */
    start_time = dbg_tmr_get() / 1000;
    
    /* Run the scheduler now to avoid uninitialized warnings */
    int skip_sched = 0;
    while (!kern.shutdown && (kern.rdy_count > 1 || kern.evblk_count > 0)) {
        uint32_t          intr;
	struct task_desc *active;

	/* Conditionally run the scheduler */
	if (!skip_sched) {
	    active = task_schedule(&kern);

#ifdef HARD_FLOAT
	    /* If the task we just scheduled has a stored floating
	       point context, save the current floating point context
	       to it's owner's stack and load up this one. */
	    if (active->fpu_ctx_on_stack) {
		vfp_enable();
		if (kern.fp_ctx_holder != NULL) {
		    /* The context holder isn't null, store their fpu context
		       on the context holder's stack. */
		    vfp_save_state(&(kern.fp_ctx_holder->fpu_regs), kern.fp_ctx_holder);
		    assert( ((unsigned int)kern.fp_ctx_holder->fpu_regs) == 
			    ((unsigned int)kern.fp_ctx_holder->regs) - 260 );
		    /* Mark that the old context holder has it's fpu state on
		       the stack */
		    kern.fp_ctx_holder->fpu_ctx_on_stack = 1;
		}

		/* Load up the active task's FPU context */
		vfp_load_state(&(active->fpu_regs));
		assert(active->fpu_regs == NULL);
		active->fpu_ctx_on_stack = 0;

		/* Change who is the context holder */
		kern.fp_ctx_holder = active;
	    } else {
		/* If we don't need to restore FPU context but
		   the task we're going to jump into does use VFP,
		   just do the re-enable. */
		if (kern.fp_ctx_holder == active) {
		    vfp_enable();
		}
	    }
#endif
	}

        time   = dbg_tmr_get() / 1000;
        intr   = ctx_switch(active);
	active->time += (dbg_tmr_get() / 1000) - time;
#ifdef HARD_FLOAT
	vfp_disable();
#endif
        skip_sched = kern_handle_intr(&kern, active, intr);

	/* Either the active task is no longer active, or we're skipping the scheduler */
	assert((TASK_STATE(active) != TASK_STATE_ACTIVE) || skip_sched);
    }

    end_time = dbg_tmr_get() / 1000;
    kern_cleanup(&kern);

    if (kp->show_top)
        kern_top(&kern, end_time - start_time);

    return 0;
}

void
kern_init(struct kern *kern, struct kparam *kp)
{
    uint32_t i, mem_avail;
    tid_t tid;

    /* Load kernel exception vector table */
    load_vector_table();

#ifdef HARD_FLOAT
    kern->fp_ctx_holder = NULL;
#endif

    /* Initialize event system */
    evt_init(&kern->eventab);

    /* Bottom of the user stacks */
    kern->user_stacks_bottom = UserStacksEnd;

    /* Find largest number of 4k pages it is safe to allocate per task */
    mem_avail = UserStacksEnd - UserStacksStart;
    kern->user_stack_size = mem_avail / PAGE_SIZE / MAX_TASKS * PAGE_SIZE;

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

int
kern_handle_intr(struct kern *kern, struct task_desc *active, uint32_t intr)
{
    switch (intr) {
    case INTR_SWI:
        kern_handle_swi(kern, active);
	return 0;
        break;
    case INTR_IRQ:
        kern_handle_irq(kern, active);
	return 0;
        break;
    case INTR_UNDEF:
        return kern_handle_undef(kern, active);
	break;
    default:
        panic("received unknown interrupt 0x%x\n\r", intr);
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
        task_ready(kern, active); /* must move out of ACTIVE state */
        break;
    case SYSCALL_PANIC:
        panic("%s\n\r", (const char*)active->regs->r0);
    default:
        panic("received unknown syscall 0x%x\n\r", syscall);
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

    /* Do any work needed for the interrupt controller,
       such as clearing the global interrupt state */
    evt_acknowledge();

    /* Return from AwaitEvent() with the result of the callback */
    wake->regs->r0 = cb_rc;
    task_ready(kern, wake);
}

int
kern_handle_undef(struct kern *k, struct task_desc *active)
{
#ifdef HARD_FLOAT
    /* If the active task is not the floating point context holder,
       it may be that they tried to execute an fpu instruction. Give them
       the floating point context and retry the instruction. If it fails again
       we know it's truely undefined. */
    if (k->fp_ctx_holder != active) {
	/* Give active the floating point context and jump back into it immediately */
	vfp_enable();
	if (k->fp_ctx_holder != NULL) {
	    /* The context holder isn't null, store their fpu context on
	       the context holder's stack. */
	    vfp_save_state(&(k->fp_ctx_holder->fpu_regs), k->fp_ctx_holder);
	    /* Mark that the old context holder has it's fpu state on the stack */
	    k->fp_ctx_holder->fpu_ctx_on_stack = 1;
	}
	/* This should only be running the first time a task tries to use FPU
	   instructions, so load a fresh FPU context */
	vfp_load_fresh();
	
	k->fp_ctx_holder = active; /* Indicate that the active now has the fp context */
	return 1; /* Don't run the scheduler so we jump right back into active */
    } else {
#endif
	/* Actual undefined instruction. Kill the process and run the scheduler. */
	bwprintf("Killing task for undefined instruction. TID: %d INSTR ADDR: %x\n\r", 
		 TASK_TID(k,active),
		 active->regs->pc);
	if(active->cleanup != NULL)
	    active->cleanup();
	task_free(k, active);
	return 0;
#ifdef HARD_FLOAT
    }
#endif
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
kern_top_pct(uint32_t total, uint32_t amt)
{
    uint32_t pct, pct10;
    pct10  = 1000 * amt / total;
    pct    = pct10 / 10;
    pct10 %= 10;
    bwprintf("\t%s%u.%u%%\n\r", pct >= 10 ? "" : " ", pct, pct10);
}

static void
kern_top(struct kern *kern, uint32_t total_time)
{
    unsigned i;
    uint32_t total_ms  = total_time / 983;
    uint32_t user_time = 0;
    bwprintf("--------\n\rran for %d ms\n\r", total_ms);
    for (i = 0; i < ARRAY_SIZE(kern->tasks); i++) {
        struct task_desc *td;
        tid_t tid;
        td = &kern->tasks[i];
        if (TASK_STATE(td) == TASK_STATE_FREE)
            continue;
        tid = TASK_TID(kern, td);
        if (tid == 0)
            bwputstr("IDLE");
        else
            bwprintf("%d", (int)tid);
        kern_top_pct(total_ms, td->time / 983);
        user_time += td->time;
    }
    bwputstr("KERNEL");
    kern_top_pct(total_ms, (total_time - user_time) / 983);
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
