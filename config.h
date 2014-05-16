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

#ifndef CONFIG_H
#define CONFIG_H

/* Maxima pertaining to task descriptors */
#define PAGE_SIZE           4096 /* All user stacks will be a multiple of this */
#define MAX_TASKS           128  /* Must be <= 254 to make room for sentinels */
#define N_PRIORITIES        16   /* Number of priorities in the system */
#define PRIORITY_MAX         0   /* Smallest priority number */
#define PRIORITY_MIN        14   /* Lowest priority number a user task can have */
#define PRIORITY_IDLE       15   /* Priority of the IDLE task */

/* Select priority queue implementation. */
#define PQ_RING
//#define PQ_HEAP

/* Application task priorities */
#define PRIORITY_NS         2 /* Name server priority */
#define PRIORITY_CLOCK      2 /* Clock server priority */
#define PRIORITY_SERIAL     2 /* Serial server priority */
#define PRIORITY_BLINK      3 /* Blink server priority */

#endif
