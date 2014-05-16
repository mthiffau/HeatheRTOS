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

#ifndef STATS_H
#define STATS_H

#include "xint.h"

/* Task running statistics */
struct stats {
    int32_t total;
    int32_t total_sqr;
    int32_t max;
    int32_t min;
    int32_t n;
};

/* Initialize statistics structure */
static inline void
stats_init(struct stats *s)
{
    s->total     = 0;
    s->total_sqr = 0;
    s->max       = (int32_t)0x80000000;
    s->min       = 0x7fffffff;
    s->n         = 0;
}

/* Accumulate a value */
static inline void
stats_accum(struct stats *s, int32_t x)
{
    assert(x >= -1000000 && x <= 1000000);
    s->total     += x;
    s->total_sqr += x * x;
    if (x > s->max)
        s->max    = x;
    if (x < s->min)
        s->min    = x;
    s->n++;
}

/* Get the mean */
static inline int32_t
stats_mean(struct stats *s)
{
    return s->total / s->n;
}

/* Get the variance */
static inline int32_t
stats_var(struct stats *s)
{
    int32_t mean = stats_mean(s);
    return s->total_sqr / s->n - mean * mean;
}

#endif
