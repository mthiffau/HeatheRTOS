#include "intr_type.h"

    .section .text

    .global ctx_switch
    .type ctx_switch, %function
    .global kern_entry_swi
    .type kern_entry_swi, %function
ctx_switch:
    @ save kernel registers
    stmfd sp!, {r0, r4, r5, r6, r7, r8, r9, r10, r11, lr}
    ldr ip, [r0]       @ get user stack pointer

    @ pop SPSR and return address from user stack
    ldmfd ip!, {r3, lr}
    msr spsr_cxsf, r3

    @ load remaining user registers and return to user mode
    ldmfd ip, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, sp, lr}^
    @ NB. may need a nop here - ARM manual says that ldm Rn, {...}^
    @ shouldn't be followed by instructions that access banked registers.
    movs pc, lr

kern_entry_swi:
    @ store ctx_switch return value in CPSR flag bits
    msr cpsr_f, #INTR_SWI_FLAGS
    @ b kern_entry  NB. add irq after this one

kern_entry:
    @ save user-mode registers on user stack
    msr cpsr_c, #0xdf  @ SYS mode, interrupts off, no thumb
    stmfd sp, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, sp, lr}
    sub ip, sp, #60    @ take note of task stack pointer
    msr cpsr_c, #0xd3  @ SVC mode, interrupts off, no thumb

    @ save spsr and return address to user stack
    mrs r3, spsr
    stmfd ip!, {r3, lr}

    @ restore ctx_switch function arguments,
    @ and write task state pointer into task descriptor
    ldr r0, [sp], #+4
    str ip, [r0]

    @ return value is stored in cpsr flags
    mrs r0, cpsr
    mov r0, r0, lsr #28
    @ restore other kernel registers and return
    ldmfd sp!, {r4, r5, r6, r7, r8, r9, r10, r11, pc}
