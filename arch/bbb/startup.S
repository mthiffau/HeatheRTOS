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

.section .text

.global _asm_entry

_asm_entry:
	/* Enable Floating Point Co-Processors */
#ifdef HARD_FLOAT
	bl vfp_cp_enable
#endif

	/* Set Kernel Stack Pointer */
	bl kernel_stack_init

	/* Set Undefined Instruction Stack Pointer */
	bl undef_instr_stack_init

	bl main    /* C code entry point */
	b .        /* loop forever */
