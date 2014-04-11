
.global _asm_entry

_asm_entry:
	ldr sp, =_MemoryEnd /* set stack pointer */
	bl main             /* C code entry point */
	b .                 /* loop forever */
