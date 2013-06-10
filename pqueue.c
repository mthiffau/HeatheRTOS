#include "config.h"
#include "xint.h"
#include "xdef.h"
#include "pqueue.h"

#include "xbool.h"
#include "xassert.h"

#ifndef PQ_RING
/* Only used for heap implementation */
static inline void pqueue_swap(struct pqueue *q, int i, int j)
{
    struct pqueue_entry tmp;
    tmp = q->nodes[i].entry;
    q->nodes[i].entry = q->nodes[j].entry;
    q->nodes[j].entry = tmp;
}
#endif

void
pqueue_init(struct pqueue *q, size_t maxsize, struct pqueue_node *mem)
{
    assert(maxsize > 0);
    q->nodes   = mem;
    q->maxsize = maxsize;
    q->count   = 0;
#ifdef PQ_RING
    q->start   = 0;
#else
    /* No PQ_HEAP-specific initialization */
#endif
}

int
pqueue_add(struct pqueue* q, intptr_t key, intptr_t val)
{
#ifdef PQ_RING
    size_t i, start;
    if (q->count == q->maxsize)
        return -1; /* no space */

    start = q->start;
    i = (start + q->count) % q->maxsize;
    while (i != start) {
        size_t prev = i == 0 ? q->maxsize - 1 : i - 1;
        if (q->nodes[prev].entry.key <= key)
            break;
        q->nodes[i].entry = q->nodes[prev].entry;
        i = prev;
    }

    q->nodes[i].entry.key = key;
    q->nodes[i].entry.val = val;
    q->count++;
    return 0;
#else
    size_t i;
    if (q->count == q->maxsize)
        return -1; /* no space */

    i = q->count++;
    q->nodes[i].entry.key = key;
    q->nodes[i].entry.val = val;

    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (q->nodes[parent].entry.key <= key)
            break;
        pqueue_swap(q, i, parent);
        i = parent;
    }
    return 0;
#endif
}

struct pqueue_entry*
pqueue_peekmin(struct pqueue *q)
{
#ifdef PQ_RING
    return q->count == 0 ? NULL : &q->nodes[q->start].entry;
#else
    return q->count == 0 ? NULL : &q->nodes[0].entry;
#endif
}

void
pqueue_popmin(struct pqueue *q)
{
#ifdef PQ_RING
    assert(q->count > 0);
    q->count--;
    q->start += 1;
    q->start %= q->maxsize;
#else
    size_t i;
    assert(q->count > 0);
    q->count--;
    pqueue_swap(q, 0, q->count);
    i = 0;
    for (;;) {
        size_t l = 2 * i + 1;
        size_t r = l + 1;
        size_t min;
        if (l >= q->count)
            break;

        min = q->nodes[l].entry.key < q->nodes[i].entry.key ? l : i;
        if (r < q->count)
            min = q->nodes[r].entry.key < q->nodes[min].entry.key ? r : min;

        if (min == i)
            break;

        pqueue_swap(q, i, min);
        i = min;
    }
#endif
}
