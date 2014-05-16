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

#ifndef SYSCALL_H
#define SYSCALL_H

/* Syscall Numbers */
#define SYSCALL_CREATE          0x0
#define SYSCALL_MYTID           0x1
#define SYSCALL_MYPARENTTID     0x2
#define SYSCALL_PASS            0x3
#define SYSCALL_EXIT            0x4
#define SYSCALL_DESTROY         0x5
#define SYSCALL_SEND            0x6
#define SYSCALL_RECEIVE         0x7
#define SYSCALL_REPLY           0x8
#define SYSCALL_REGISTERCLEANUP 0x9
#define SYSCALL_REGISTEREVENT   0xa
#define SYSCALL_AWAITEVENT      0xb
#define SYSCALL_SHUTDOWN        0xc
#define SYSCALL_PANIC           0xd

#endif
