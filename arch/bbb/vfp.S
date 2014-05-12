.section .text

#ifdef HARD_FLOAT	
.global vfp_cp_enable
.type vfp_cp_enable, %function
.global vfp_cp_disable
.type vfp_cp_disable, %function
.global vfp_enable
.type vfp_enable, %function
.global vfp_disable
.type vfp_disable, %function
.global vfp_save_state
.type vfp_save_state, %function
.global vfp_load_state
.type vfp_load_state, %function
.global vfp_load_fresh
.type vfp_load_fresh, %function

vfp_cp_enable_mask:
	.word 0x00F00000
vfp_cp_disable_mask:
	.word 0xFF0FFFFF
	
vfp_enable_mask:
	.word 0x40000000
vfp_disable_mask:
	.word 0xBFFFFFFF

vfp_cp_enable:
	/* Grant access to the cp{10,11} coprocessors */
	/* Set CPACR.{cp10,cp11} to be 0b11 */
	mrc p15, 0, r0, c1, c0, 2 // Read CPACR
	ldr r1, =vfp_cp_enable_mask
	ldr r2, [r1]
	orr r0, r0, r2
	mcr p15, 0, r0, c1, c0, 2 // Write CPACR

	/* Memory/Data barrier just in case */
	dsb
	isb
	bx lr

vfp_cp_disable:
	/* Disable access to the cp{10,11} coprocessors */
	/* Set CPACR.{cp10,cp11} to be 0b00 */
	mrc p15, 0, r0, c1, c0, 2 // Read CPACR
	ldr r1, =vfp_cp_disable_mask
	ldr r2, [r1]
	and r0, r0, r2
	mcr p15, 0, r0, c1, c0, 2 // Write CPACR

	/* Memory/Data barrier just in case */
	dsb
	isb
	bx lr

vfp_enable:
	/* Enable VFP */
	/* Set FPEXC.EN to 1*/
	vmrs r0, FPEXC
	ldr r1, =vfp_enable_mask
	ldr r2, [r1]
	orr r0, r0, r2
	vmsr FPEXC, r0

	bx lr

vfp_disable:
	/* Set FPEXC.EN to 0*/
	vmrs r0, FPEXC
	ldr r1, =vfp_disable_mask
	ldr r2, [r1]
	and r0, r0, r2
	vmsr FPEXC, r0

	bx lr

vfp_save_state:
        /* Save the VFP context to the task stack */
	ldr r2, [r1]
	
	/* Store the register file */
	vstmdb.64 r2!, {D16-D31}
	vstmdb.64 r2!, {D0-D15}

	/* Get the status register */
	vmrs r3, FPSCR
	str r3, [r2, #-4]!

	/* Write the stack location into the fpu_regs pointer */
	str r2, [r0]

	bx lr

vfp_load_state:
	/* Restore the VFP context from the given pointer */
	
	/* Get the fpu_regs pointer */
	ldr r1, [r0]

	/* Restore the status register */
	ldr r2, [r1], #+4
	vmsr FPSCR, r2

	/* Restore the register file */
	vldmia.64 r1!, {D0-D15}
	vldmia.64 r1!, {D16-D31}

	/* Set the fpu_regs pointer to null */
	mov r1, #0
	str r1, [r0]

	bx lr

vfp_load_fresh:
	/* Load a fresh state into the fpu context */
	push {r1}
	ldr r0, =blank_vfp_regs
	vldmia.64 r0, {D0-D15}
	vldmia.64 r0, {D16-D31}
	
	ldr r0, =blank_vfp_SPSCR
	ldr r1, [r0]
	vmsr FPSCR, r1

	pop {r1}
	bx lr

blank_vfp_SPSCR:
	.word 0x01000000
blank_vfp_regs:
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
#endif
