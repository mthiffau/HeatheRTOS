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

#include "xint.h"
#include "xrand.h"

#include "xbool.h"
#include "xassert.h"
#include "timer.h"

#define RAND_SPAN   (RAND_MAX + 1)

/* Linear congruential generator:
 *     https://en.wikipedia.org/wiki/Linear_congruential_generator
 *
 * Constants used are those listed under Watcom. */

/* Seed RNG */
void
rand_init(struct rand *r, uint32_t seed)
{
    r->seed = seed;
}

/* Seed the RNG using the debug timer */
void
rand_init_time(struct rand *r)
{
    rand_init(r, dbg_tmr_get());
}

/* Get a pseudo random integer */
uint32_t
rand(struct rand *r)
{
    const int a = 1103515245;
    const int c = 12345;
    r->seed *= a;
    r->seed += c;
    r->seed &= RAND_MAX; /* RAND_MAX is of the form 2^k-1 */
    return r->seed;
}

/* Get a pseudo random integer in the given range */
uint32_t
randrange(struct rand *r, uint32_t a, uint32_t b)
{
    /* Use float to avoid overflow. */
    float x;
    assert(a < b);
    x  = (float)rand(r) / (float)RAND_SPAN; /* in [0, 1)   */
    x *= (float)(b - a);                    /* in [0, b-a) */
    x += (float)a;                          /* in [a, b)   */
    return (uint32_t)x;
}
