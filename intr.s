    .section .text

    .align 4
    .global swi_test
    .type   swi_test, %function
swi_test:
    swi #0xddbeef
    mov pc, lr
