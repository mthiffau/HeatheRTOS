#include "syscall.h"

    .section .text
    .align 4

    .global Create
    .type   Create, %function
Create:
    swi #SYSCALL_CREATE
    mov pc, lr

    .global MyTid
    .type   MyTid, %function
MyTid:
    swi #SYSCALL_MYTID
    mov pc, lr

    .global MyParentTid
    .type   MyParentTid, %function
MyParentTid:
    swi #SYSCALL_MYPARENTTID
    mov pc, lr

    .global Pass
    .type   Pass, %function
Pass:
    swi #SYSCALL_PASS
    mov pc, lr

    .global Exit
    .type   Exit, %function
Exit:
    swi #SYSCALL_EXIT
    mov pc, lr
