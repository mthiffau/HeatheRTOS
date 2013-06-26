#ifdef STATS_H
#error "double-included stats.h"
#endif

#define STATS_H

XINT_H;

struct stats {
    int32_t total;
    int32_t total_sqr;
    int32_t max;
    int32_t min;
    int32_t n;
};

static inline void
stats_init(struct stats *s)
{
    s->total     = 0;
    s->total_sqr = 0;
    s->max       = (int32_t)0x80000000;
    s->min       = 0x7fffffff;
    s->n         = 0;
}

static inline void
stats_accum(struct stats *s, int32_t x)
{
    assert(x >= -1000000 && x <= 1000000);
    s->total     += x;
    s->total_sqr += x * x;
    if (x > s->max)
        s->max    = x;
    if (x < s->min)
        s->min    = x;
    s->n++;
}

static inline int32_t
stats_mean(struct stats *s)
{
    return s->total / s->n;
}

static inline int32_t
stats_var(struct stats *s)
{
    int32_t mean = stats_mean(s);
    return s->total_sqr / s->n - mean * mean;
}
