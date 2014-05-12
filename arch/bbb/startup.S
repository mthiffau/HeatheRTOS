.section .text

.global _asm_entry

_asm_entry:
	/* Enable Floating Point Co-Processors */
#ifdef HARD_FLOAT
	bl vfp_cp_enable
#endif

	/* Set Kernel Stack Pointer */
	bl kernel_stack_init

	/* Set Undefined Instruction Stack Pointer */
	bl undef_instr_stack_init

	bl main    /* C code entry point */
	b .        /* loop forever */
