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

#ifndef XASSERT_H
#define XASSERT_H

#include "xbool.h"

#ifdef NOASSERT
#define assertv(v,x)    ((void)(v))
#define assert(x)       assertv(0, x)
#else
#define assertv(v,x)    assert(x)
#define assert(x)       _assert((x), __FILE__, __LINE__, #x)
#endif

void panic(const char *fmt, ...)
    __attribute__((noreturn, format(printf, 1, 2)));
void _assert(bool x, const char *file, int line, const char *expr);

#endif
