#include "xint.h"
#include "xrand.h"

#include "xbool.h"
#include "timer.h"

/* Linear congruential generator:
 *     https://en.wikipedia.org/wiki/Linear_congruential_generator
 *
 * Constants used are those listed under Watcom. */

void
rand_init(struct rand *r, uint32_t seed)
{
    r->seed = seed;
}

void
rand_init_time(struct rand *r)
{
    rand_init(r, tmr40_get());
}

uint32_t
rand(struct rand *r)
{
    const int a = 1103515245;
    const int c = 12345;
    r->seed *= a;
    r->seed += c;
    r->seed &= 0x7fffffff;
    return r->seed;
}
