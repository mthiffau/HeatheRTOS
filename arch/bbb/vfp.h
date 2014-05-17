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

#ifndef VFP_ENABLE_H
#define VFP_ENABLE_H

#include "task.h"

void vfp_cp_enable(void);
void vfp_cp_disable(void);
void vfp_enable(void);
void vfp_disable(void);

void vfp_save_state(volatile struct task_fpu_regs**, void* stack_pointer);
void vfp_load_state(volatile struct task_fpu_regs**);
void vfp_load_fresh(void);

#endif
