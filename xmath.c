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
