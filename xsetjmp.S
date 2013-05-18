    .section .text

    .align  4
    .global setjmp
    .type   setjmp, %function
setjmp:
    stmia r0, {r4, r5, r6, r7, r8, r9, r10, r11, sp, lr}
    mov r0, #0
    mov pc, lr

    .global longjmp
    .type   longjmp, %function
longjmp:
    mov r2, r0
    mov r0, r1
    ldmia r2, {r4, r5, r6, r7, r8, r9, r10, r11, sp, pc}
