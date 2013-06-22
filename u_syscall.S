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

    .global Send
    .type   Send, %function
Send:
    swi #SYSCALL_SEND
    mov pc, lr

    .global Receive
    .type   Receive, %function
Receive:
    swi #SYSCALL_RECEIVE
    mov pc, lr

    .global Reply
    .type   Reply, %function
Reply:
    swi #SYSCALL_REPLY
    mov pc, lr

    .global RegisterCleanup
    .type   RegisterCleanup, %function
RegisterCleanup:
    swi #SYSCALL_REGISTERCLEANUP
    mov pc, lr

    .global RegisterEvent
    .type   RegisterEvent, %function
RegisterEvent:
    swi #SYSCALL_REGISTEREVENT
    mov pc, lr

    .global AwaitEvent
    .type   AwaitEvent, %function
AwaitEvent:
    swi #SYSCALL_AWAITEVENT
    mov pc, lr

    .global Shutdown
    .type   Shutdown, %function
Shutdown:
    swi #SYSCALL_SHUTDOWN
    mov pc, lr
