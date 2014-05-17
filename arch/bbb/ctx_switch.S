/*******************************************************************************
    Copyright 2014 Matthew Thiffault

    This file is part of HeatheRTOS.

    HeatheRTOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    HeatheRTOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HeatheRTOS.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************/

#include "intr_type.h"

    .section .text

    .global ctx_switch
    .type ctx_switch, %function
    .global kern_entry_swi
    .type kern_entry_swi, %function
    .global kern_entry_irq
    .type kern_entry_irq, %function
    .global kern_entry_undef
    .type kern_entry_undef, %function

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
    @ save user-mode registers on user stack
    msr cpsr_c, #0xdf    @ SYS mode, interrupts off, no thumb
    stmfd sp, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, sp, lr}
    sub ip, sp, #60      @ take note of task stack pointer

    @ return to initial mode to read LR, SPSR (interrupts off, no thumb)
    @ NB. V is set if we came in through kern_entry_irq
    msr cpsr_c, #0xd3  @ SVC mode

    @ save spsr and return address to user stack
    mrs r3, spsr
    stmfd ip!, {r3, lr}

    @ restore ctx_switch function arguments,
    @ and write task state pointer into task descriptor
    ldr r0, [sp], #+4
    str ip, [r0]

    @ return value is stored in cpsr flags
    mov r0, #INTR_SWI
    @ restore other kernel registers and return
    ldmfd sp!, {r4, r5, r6, r7, r8, r9, r10, r11, pc}

kern_entry_irq:
    sub lr, lr, #4

    @ save user-mode registers on user stack
    msr cpsr_c, #0xdf    @ SYS mode, interrupts off, no thumb
    stmfd sp, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, sp, lr}
    sub ip, sp, #60      @ take note of task stack pointer

    @ return to initial mode to read LR, SPSR (interrupts off, no thumb)
    @ NB. V is set if we came in through kern_entry_irq
    msr cpsr_c, #0xd2  @ IRQ mode

    @ save spsr and return address to user stack
    mrs r3, spsr
    stmfd ip!, {r3, lr}

    @ return to SVC mode to prepare for return to kernel -
    @ this is only really necessary in the IRQ case
    msr cpsr_c, #0xd3  @ SVC mode

    @ restore ctx_switch function arguments,
    @ and write task state pointer into task descriptor
    ldr r0, [sp], #+4
    str ip, [r0]

    @ return value is stored in cpsr flags
    mov r0, #INTR_IRQ
    @ restore other kernel registers and return
    ldmfd sp!, {r4, r5, r6, r7, r8, r9, r10, r11, pc}

kern_entry_undef:
    sub lr, lr, #4

    @ save user-mode registers on user stack
    msr cpsr_c, #0xdf    @ SYS mode, interrupts off, no thumb
    stmfd sp, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, sp, lr}
    sub ip, sp, #60      @ take note of task stack pointer

    @ return to initial mode to read LR, SPSR (interrupts off, no thumb)
    msr cpsr_c, #0xdb  @ UNDEF mode

    @ save spsr and return address to user stack
    mrs r3, spsr
    stmfd ip!, {r3, lr}

    @ return to SVC mode to prepare for return to kernel -
    @ this is only really necessary in the IRQ case
    msr cpsr_c, #0xd3  @ SVC mode

    @ restore ctx_switch function arguments,
    @ and write task state pointer into task descriptor
    ldr r0, [sp], #+4
    str ip, [r0]

    @ return value is stored in cpsr flags
    mov r0, #INTR_UNDEF
    @ restore other kernel registers and return
    ldmfd sp!, {r4, r5, r6, r7, r8, r9, r10, r11, pc}
