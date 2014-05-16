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

#ifndef XRAND_H
#define XRAND_H

#include "xint.h"

#define RAND_MAX    0x7fffffffu

struct rand {
    uint32_t seed;
};

/* Initialize a random number generator state with the given seed. */
void rand_init(struct rand *r, uint32_t seed);

/* Initialize a random number generator state with the debug timer as seed. */
void rand_init_time(struct rand *r);

/* Generate the next pseudo-random number in the sequence. */
uint32_t rand(struct rand *r);

/* Generate a pseudo-random number in [a, b). */
uint32_t randrange(struct rand *r, uint32_t a, uint32_t b);

#endif
