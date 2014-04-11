    .section .text
    .align   4

    .global cache_enable
    .type cache_enable, %function
cache_enable:
    mov r0, #0
    mcr p15, 0, r0, c7, c5     @ invalidate entire I-cache
    mcr p15, 0, r0, c7, c6     @ invalidate entire D-cache
    mrc p15, 0, r0, c1, c0     @ read control register
    orr r0, r0, #0x1000        @ enable I-cache
    orr r0, r0, #0x4           @ enable D-cache
    mcr p15, 0, r0, c1, c0     @ write back control register
    mov pc, lr

    .global cache_disable
    .type cache_disable, %function
cache_disable:
    mrc p15, 0, r0, c1, c0
    bic r0, r0, #0x1000        @ disable I-cache
    bic r0, r0, #0x4           @ disable D-cache
    mcr p15, 0, r0, c1, c0     @ write back control register
    mov r0, #0
    mcr p15, 0, r0, c7, c10    @ clean entire D-cache
    mov pc, lr

    .global cache_typereg
    .type cache_typereg, %function
l1cache_typereg:
    mrc p15, 0, R0, c0, c0, 1
    mov pc, lr
