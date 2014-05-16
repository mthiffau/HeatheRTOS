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

#ifndef XDEF_H
#define XDEF_H

/* Define NULL */
#define NULL ((void*)0)

/* Define size types */
typedef long          ssize_t;
typedef unsigned long size_t;

/* Macro to statically determine the offset of a field
   in a structure */
#define offsetof(st, m) ((size_t)(&((st*)0)->m))

#endif
