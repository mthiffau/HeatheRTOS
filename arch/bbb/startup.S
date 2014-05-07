.section .text

.global _asm_entry

_stack_bottom:
	.word 0x4030FFF0

_asm_entry:
	/* Enable Floating Point Co-Processors */
	bl vfp_cp_enable

	/* Set the stack pointer and jump into kernel */
	ldr r0, =_stack_bottom
	ldr r1, [r0]
	mov sp, r1 /* set stack pointer */
	bl main    /* C code entry point */
	b .        /* loop forever */
