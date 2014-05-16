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

#include "xmath.h"

#include "xbool.h"
#include "xassert.h"

/* Calculate a non-negative integer a^n */
float
powi(float a, int n)
{
    float pow;
    assert(n >= 0);

    /* Square and multiply */
    if (n == 0)
        return 1.f;
    else if (n == 1)
        return a;

    pow  = powi(a, n / 2);
    pow *= pow;
    if (n % 2 == 1)
        pow *= a;

    return pow;
}

/* Find the nth root of a number */
float
nthroot(int n, float a, int maxiter)
{
    /* Go go gadget Newton's method! */
    int i;
    float x = 1.f, nf = (float)n;
    for (i = 0; i < maxiter; i++)
        x = ((nf - 1) * a * powi(x, n) + 1) / (a * nf * powi(x, n-1));
    return 1.f / x;
}
