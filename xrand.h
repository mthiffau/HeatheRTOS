#ifndef XRAND_H
#define XRAND_H

#include "xint.h"

#define RAND_MAX    0x7fffffffu

struct rand {
    uint32_t seed;
};

/* Initialize a random number generator state with the given seed. */
void rand_init(struct rand *r, uint32_t seed);

/* Initialize a random number generator state with the debug timer as seed. */
void rand_init_time(struct rand *r);

/* Generate the next pseudo-random number in the sequence. */
uint32_t rand(struct rand *r);

/* Generate a pseudo-random number in [a, b). */
uint32_t randrange(struct rand *r, uint32_t a, uint32_t b);

#endif
