#ifndef XMATH_H
#define XMATH_H

/* Calculate a non-negative integer power a^n. */
float powi(float a, int n);

/* Calculate the nth root of a,
 * using at most maxiter iterations of Newton's method. */
float nthroot(int n, float a, int maxiter);

#endif
