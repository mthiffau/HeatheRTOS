#ifdef POLY_H
#error "double-included poly.h"
#endif

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
float polyeval(struct poly *p, float t);

/* Differentiate p to yield p_prime. */
void polydiff(struct poly *p, struct poly *p_prime);

/* Find a root of p(t) using Newton's method.
 *
 * t0       initial estimate
 * maxiter  iteration limit
 * eps      desired error bound
 *
 * Returns the final estimate of t. It's possible that |p(t)| >= eps
 * in the case that the iteration limit is exceeded. */
float polyroot(struct poly *p, float t0, float eps, int maxiter);

/* Find t such that p(t)=x. Further parameters as in polyroot. */
float polyinv(struct poly *p, float x, float t0, float eps, int maxiter);
