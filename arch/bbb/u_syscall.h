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

#ifndef U_SYSCALL_H
#define U_SYSCALL_H

#include "xdef.h"
#include "u_tid.h"

tid_t Create(int priority, void (*task_entry)(void));
tid_t MyTid(void);
tid_t MyParentTid(void);
void  Pass(void);
void  Exit(void) __attribute__((noreturn));

int   Send(int TID, const void* msg, int msglen, void* reply, int replylen);
int   Receive(int* TID, void* msg, int msglen);
int   Reply(int TID, const void* reply, int replylen);

void  RegisterCleanup(void (*cleanup_cb)(void));

int   RegisterEvent(int irq, int (*cb)(void*, size_t));
int   AwaitEvent(void*, size_t);

/* Instruct the kernel to shut down immediately. */
void Shutdown(void) __attribute__((noreturn));
void Panic(const char *msg) __attribute__((noreturn));

#endif
