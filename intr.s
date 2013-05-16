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
    @ nothing specific yet
    b exch_common

exch_common:
    @ save user registers and return address in struct taskdesc
    ldr sp, [pc, #+20]  @ curtask
    ldr sp, [sp]
    stmia sp, {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,lr}^
    str lr, [sp, #+60]

    @ TODO FIXME get SPSR

    @ load fixed kernel stack address and jump to kernel event handling code
    ldr sp, [pc, #+8]   @ kern_exit
    ldr sp, [sp, #+32]
    b kern_event

    @ global addresses
    .word curtask
    .word kern_exit
