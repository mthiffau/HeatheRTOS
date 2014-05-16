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

#ifndef POLY_H
#define POLY_H

#define POLY_MAX_DEGREE     4

/* A polynomial in a single variable t:
 * p(t) = a[0] + a[1] * t + a[2] * t^2 + ... + a[deg] * t^deg
 */
struct poly {
    int   deg;                    /* degree of the polynomial */
    float a[POLY_MAX_DEGREE + 1]; /* coefficients */
};

/* Evaluate p(t). */
float polyeval(const struct poly *p, float t);

/* Differentiate p to yield p_prime. */
void polydiff(const struct poly *p, struct poly *p_prime);

/* Find a root of p(t) using Newton's method.
 *
 * t0       initial estimate
 * maxiter  iteration limit
 * eps      desired error bound
 *
 * Returns the final estimate of t. It's possible that |p(t)| >= eps
 * in the case that the iteration limit is exceeded. */
float polyroot(const struct poly *p, float t0, float eps, int maxiter);

/* Find t such that p(t)=x. Further parameters as in polyroot. */
float polyinv(const struct poly *p, float x, float t0, float eps, int maxiter);

#endif
