    .section .text

    .align 4
    .global trigger_swi
    .type   trigger_swi, %function
trigger_swi:
    swi #0xddbeef
    mov pc, lr

    .global ctx_switch
    .type ctx_switch, %function
    .global kern_entry_swi
    .type kern_entry_swi, %function
ctx_switch:
    stmfd sp!, {r0, r1, r4, r5, r6, r7, r8, r9, r10, r11, lr}
    ldr ip, [r0]       @ get user stack pointer
    ldr r0, [ip], #+4  @ read SPSR from user stack
    msr spsr, r0
    msr cpsr_c, #0xdf  @ SYS mode, interrupts off, no thumb
    add sp, ip, #60
    msr cpsr_c, #0xd3  @ SVC mode, interrupts off, no thumb
    ldmfd ip, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr, pc}^
kern_entry_swi:
    @ b kern_entry  NB. most common interrupt should be last
kern_entry:
    msr cpsr_c, #0xdf  @ SYS mode, interrupts off, no thumb
    sub sp, sp, #4
    stmfd sp!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr}
    mov ip, sp
    msr cpsr_c, #0xd3  @ SVC mode, interrupts off, no thumb
    str lr, [ip, #+56]
    mrs r3, spsr
    str r3, [ip, #-4]!
    ldmfd sp!, {r0, r1}
    str ip, [r0]
    ldmfd sp!, {r4, r5, r6, r7, r8, r9, r10, r11, pc}
