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

.section .text

.global kernel_stack_init
.type kernel_stack_init, %function
.global undef_instr_stack_init
.type undef_instr_stack_init, %function

_kernel_stack_bottom:
	.word 0x4030FBF0

_undef_instr_stack_bottom:
	.word 0x4030FFF0

kernel_stack_init:
	ldr r0, =_kernel_stack_bottom
	ldr r1, [r0]
	mov sp, r1

	bx lr

undef_instr_stack_init:
	msr cpsr_c, #0xdb /* Go into Undef Mode */
	ldr r0, =_undef_instr_stack_bottom
	ldr r1, [r0]
	mov sp, r1
	msr cpsr_c, #0xd3 /* Back to SVC Mode */
	
	bx lr
