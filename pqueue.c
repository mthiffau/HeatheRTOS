#include "config.h"
#include "xint.h"
#include "xdef.h"
#include "pqueue.h"

#include "xbool.h"
#include "xassert.h"

void
pqueue_init(struct pqueue *q, size_t maxsize, struct pqueue_node *mem)
{
    assert(maxsize > 0);
    q->nodes   = mem;
    q->maxsize = maxsize;
#ifdef PQ_RING
    q->start = 0;
    q->count = 0;
#else
#error "heap priority queue not implemented"
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
#error "heap priority queue not implemented"
#endif
}

struct pqueue_entry*
pqueue_peekmin(struct pqueue *q)
{
#ifdef PQ_RING
    return q->count == 0 ? NULL : &q->nodes[q->start].entry;
#else
#error "heap priority queue not implemented"
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
#error "heap priority queue not implemented"
#endif
}
