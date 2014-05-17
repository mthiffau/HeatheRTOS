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

#ifndef LINK_H
#define LINK_H

/* Declarations for memory locations defined by the linker script */
#define ExcVecLoc ((void*)(&_ExcVecLoc))
extern char _ExcVecLoc;

#define KernStackTop ((void*)(&_KernStackTop))
extern char _KernStackTop;

#define KernStackBottom ((void*)(&_KernStackBottom))
extern char _KernStackBottom;

#define UserStacksStart ((void*)(&_UserStacksStart))
extern char _UserStacksStart;

#define UserStacksEnd ((void*)(&_UserStacksEnd))
extern char _UserStacksEnd;

#define UndefInstrStackTop ((void*)(&_UndefInstrStackTop))
extern char _UndefInstrStackTop;

#define UndefInstrStackBottom ((void*)(&_UndefInstrStackBottom))
extern char _UndefInstrStackBottom;

#endif
