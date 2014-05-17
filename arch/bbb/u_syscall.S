/*******************************************************************************
    Copyright 2014 Matthew Thiffault

    This file is part of HeatheRTOS.

    HeatheRTOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    HeatheRTOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HeatheRTOS.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************/

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

    .global Panic
    .type   Panic, %function
Panic:
    swi #SYSCALL_PANIC
    mov pc, lr
