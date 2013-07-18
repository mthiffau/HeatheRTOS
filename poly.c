#include "poly.h"

#include "xbool.h"
#include "xassert.h"

float
polyeval(const struct poly *p, float t)
{
    float x, tp = 1.f;
    int i;
    assert(p->deg >= 0);
    x = p->a[0];
    for (i = 1; i <= p->deg; i++) {
        tp *= t;
        x += p->a[i] * tp;
    }
    return x;
}

void
polydiff(const struct poly *p, struct poly *p_prime)
{
    assert(p->deg >= 0);
    if (p->deg == 0) {
        p_prime->deg  = 0;
        p_prime->a[0] = 0.f;
    } else {
        int i;
        p_prime->deg = p->deg - 1;
        for (i = 1; i <= p->deg; i++)
            p_prime->a[i-1] = (float)i * p->a[i];
    }
}

float
polyroot(const struct poly *p, float t0, float eps, int maxiter)
{
    /* Newton's method */
    int i;
    struct poly p_prime;
    polydiff(p, &p_prime);
    for (i = 0; i < maxiter; i++) {
        float x = polyeval(p, t0);
        if (x > -eps && x < eps)
            break;
        t0 -= x / polyeval(&p_prime, t0);
    }
    return t0;
}

float
polyinv(const struct poly *p, float x, float t0, float eps, int maxiter)
{
    struct poly q = *p;
    q.a[0] -= x;
    return polyroot(&q, t0, eps, maxiter);
}
