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

#ifndef CTX_SWITCH_H
#define CTX_SWITCH_H

#include "xint.h"
#include "task.h"

/* Kernel entry for software interrupts */
void     kern_entry_swi(void);
/* Kernel entry for IRQ handling */
void     kern_entry_irq(void);
/* Kernel entry for undefined instructions */
void     kern_entry_undef(void);
/* Context switch into a task, return trap reason */
uint32_t ctx_switch(struct task_desc *td);

#endif
