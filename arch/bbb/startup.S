
.global _asm_entry

.equ    vfp_enable, 0x40000000
	
_asm_entry:
	/* Enable VFP */
	/* Set CPACR.{cp10,cp11} to be 0b11 */
	mrc p15, 0, r0, c1, c0, 2 // Read CPACR
	orr r0, r0, #0x00F00000
	mcr p15, 0, r0, c1, c0, 2 // Write CPACR

	/* Set FPEXC.EN to 1*/
	vmrs r0, FPEXC
	orr r0, r0, #vfp_enable
	vmsr FPEXC, r0

	/* Memory/Data barrier just in case */
	dsb
	isb

	/* Set the stack pointer and jump into kernel */
	ldr sp, =_MemoryEnd /* set stack pointer */
	bl main             /* C code entry point */
	b .                 /* loop forever */
