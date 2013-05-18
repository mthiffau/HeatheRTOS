#include "syscall.h"

    .section .text

    .align 4
    .global Pass
    .type   Pass, %function
Pass:
    swi #SYSCALL_PASS
    mov pc, lr
