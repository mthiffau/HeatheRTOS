.section .text

.global vfp_cp_enable
.type vfp_cp_enable, %function
.global vfp_cp_disable
.type vfp_cp_disable, %function
.global vfp_enable
.type vfp_enable, %function
.global vfp_disable
.type vfp_disable, %function

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
	orr r0, r0, r2
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

	/* Memory/Data barrier just in case */
	dsb
	isb
	bx lr

vfp_disable:
	/* Set FPEXC.EN to 1*/
	vmrs r0, FPEXC
	ldr r1, =vfp_disable_mask
	ldr r2, [r1]
	orr r0, r0, r2
	vmsr FPEXC, r0

	/* Memory/Data barrier just in case */
	dsb
	isb
	bx lr
