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
    @ save kernel registers
    stmfd sp!, {r0, r4, r5, r6, r7, r8, r9, r10, r11, lr}
    ldr ip, [r0]       @ get user stack pointer
    ldr r0, [ip], #+4  @ read SPSR from user stack

    @ set user sp register
    msr spsr, r0
    msr cpsr_c, #0xdf  @ SYS mode, interrupts off, no thumb
    add sp, ip, #60
    msr cpsr_c, #0xd3  @ SVC mode, interrupts off, no thumb

    @ load remaining user registers and return to user mode
    ldmfd ip, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr, pc}^

kern_entry_swi:
    msr cpsr_f, #0xa0000000  @ set flags to 0xa (SWI)
    @ b kern_entry  NB. most common interrupt should be last

kern_entry:
    @ save user-mode registers on user stack, except for sp
    msr cpsr_c, #0xdf  @ SYS mode, interrupts off, no thumb
    sub sp, sp, #4     @ space for PC, need to write it in SVC mode
    stmfd sp!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr}
    mov ip, sp         @ take note of task stack pointer
    msr cpsr_c, #0xd3  @ SVC mode, interrupts off, no thumb

    @ store return address in reserved PC space
    str lr, [ip, #+56]

    @ push SPSR onto user stack
    mrs r3, spsr
    str r3, [ip, #-4]!

    @ restore ctx_switch function arguments,
    @ and write task state pointer into task descriptor
    ldr r0, [sp], #+4
    str ip, [r0]

    @ return value is stored in cpsr flags
    mrs r0, cpsr
    mov r0, r0, lsr #28
    @ restore other kernel registers and return
    ldmfd sp!, {r4, r5, r6, r7, r8, r9, r10, r11, pc}
