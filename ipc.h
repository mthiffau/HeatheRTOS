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

#ifndef IPC_H
#define IPC_H

#include "task.h"
#include "kern.h"

/* Immediate work for Send() system call. */
void ipc_send_start(struct kern *kern, struct task_desc *active);

/* Immediate work for Receive() system call. */
void ipc_receive_start(struct kern *kern, struct task_desc *active);

/* Immediate work for Reply() system call. */
void ipc_reply_start(struct kern *kern, struct task_desc *active);

#endif
