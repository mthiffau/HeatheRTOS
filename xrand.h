#ifdef XRAND_H
#error "double-included xrand.h"
#endif

#define XRAND_H

XINT_H;

struct rand {
    uint32_t seed;
};

void rand_init(struct rand *r, uint32_t seed);
void rand_init_time(struct rand *r);
uint32_t rand(struct rand *r);
