    .section .text

    .extern curtask
    .extern kern_exit
    .extern kern_event

    .align 4
    .global swi_test
    .type   swi_test, %function
swi_test:
    swi #0xddbeef
    mov pc, lr

    .global activate_ctx
    .type activate_ctx, %function
activate_ctx:
    mov ip, #0x10 @ FIXME restore the correct PSR
    msr spsr, ip
    ldmia r0, {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,lr,pc}^

    .global exch_swi
    .type exch_swi, %function
exch_swi:
    @ save user registers
    ldr sp, [pc, #+28]  @ curtask
    ldr sp, [sp]
    stmia sp, {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,lr}^

    @ read swi instruction into r0 (kern_event first instruction)
    ldr r0, [lr, #-4]
    bic r0, r0, #0xff000000

    @ save return address
    str lr, [sp, #+60]

    @ TODO FIXME get SPSR

    @ load fixed kernel stack pointer and handle event
    ldr sp, [pc, #+8]   @ kern_exit
    ldr sp, [sp, #+32]
    b kern_event

    @ global addresses
    .word curtask
    .word kern_exit
