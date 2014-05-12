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
