.section .text

.global kernel_stack_init
.type kernel_stack_init, %function
.global undef_instr_stack_init
.type undef_instr_stack_init, %function

_kernel_stack_bottom:
	.word 0x4030FF00

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
