    .section .text

    .global memcpy
    .type memcpy, %function
memcpy:
    stmfd sp!, {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12}
    ands r3, r0, #0x00000003
    bne check_saveable
    ands r3, r1, #0x00000003
    bne copy_rest
    b burst_cpy40

check_saveable:
    and r4, r1, #0x00000003
    cmp r3,r4
    beq fix_alignment
    b copy_rest

fix_alignment:
    sub r2, r2, #1
    ldrb r3, [r1], #1
    strb r3, [r0], #1
    ands r3, r0, #0x00000003
    beq burst_cpy40
    b fix_alignment

burst_cpy40:
    subs r3, r2, #40
    blt burst_cpy20
    sub r2, r2, #40
    ldmia r1!, {r3,r4,r5,r6,r7,r8,r9,r10,r11,r12}
    stmia r0!, {r3,r4,r5,r6,r7,r8,r9,r10,r11,r12}
    b burst_cpy40

burst_cpy20:
    subs r3, r2, #20
    blt burst_cpy12
    sub r2, r2, #20
    ldmia r1!, {r3,r4,r5,r6,r7}
    stmia r0!, {r3,r4,r5,r6,r7}
    b burst_cpy20

burst_cpy12:
    subs r3, r2, #12
    blt burst_cpy4
    sub r2, r2, #12
    ldmia r1!, {r3,r4,r5}
    stmia r0!, {r3,r4,r5}
    b burst_cpy12

burst_cpy4:
    subs r3, r2, #4
    blt copy_rest
    sub r2, r2, #4
    ldr r3, [r1], #4
    str r3, [r0], #4
    b burst_cpy4

copy_rest:
    subs r3, r2, #1
    blt memcpy_done
    sub r2, r2, #1
    ldrb r3, [r1], #1
    strb r3, [r0], #1
    b copy_rest

memcpy_done:
    ldmfd sp!, {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12}
    mov pc, lr

