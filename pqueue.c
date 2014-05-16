/*******************************************************************************
    Copyright 2014 Matthew Thiffault

    This file is part of HeatheRTOS.

    HeatheRTOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    HeatheRTOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HeatheRTOS.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************/

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
    q->nodes[q->nodes[i].entry.val].pos = i;
    q->nodes[q->nodes[j].entry.val].pos = j;
}
#endif

/* Initialize a priority queue */
void
pqueue_init(struct pqueue *q, size_t maxsize, struct pqueue_node *mem)
{
    size_t i;
    assert(maxsize > 0);
    q->nodes   = mem;
    q->maxsize = maxsize;
    q->count   = 0;
    for (i = 0; i < maxsize; i++)
        q->nodes[i].pos = maxsize;
#ifdef PQ_RING
    q->start   = 0;
#else
    /* No PQ_HEAP-specific initialization */
#endif
}

/* Add a value to a priority queue */
int
pqueue_add(struct pqueue* q, size_t val, intptr_t key)
{
    size_t i;
#ifdef PQ_RING
    size_t start;
#endif
    if (val >= q->maxsize)
        return -1; /* value too large */
    if (q->nodes[val].pos < q->maxsize)
        return -2; /* already exists */

    /* Should be impossible to more than fill the queue given
     * constraints on values stored in the queue. */
    assert (q->count < q->maxsize);

#ifdef PQ_RING
    start = q->start;
    i = (start + q->count) % q->maxsize;
    while (i != start) {
        size_t prev = i == 0 ? q->maxsize - 1 : i - 1;
        if (q->nodes[prev].entry.key <= key)
            break;
        q->nodes[i].entry = q->nodes[prev].entry;
        q->nodes[q->nodes[i].entry.val].pos = i;
        i = prev;
    }

    q->nodes[i].entry.key = key;
    q->nodes[i].entry.val = val;
    q->nodes[val].pos     = i;
    q->count++;
    return 0;
#else
    i = q->count++;
    q->nodes[i].entry.key = key;
    q->nodes[i].entry.val = val;
    q->nodes[val].pos = i;

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

/* Reduce the key of a value in the priority queue */
int
pqueue_decreasekey(struct pqueue *q, size_t val, intptr_t new_key)
{
#ifdef PQ_RING
    size_t start;
#endif
    size_t i = q->nodes[val].pos;
    if (i == q->maxsize)
        return -1; /* value not in queue */

    assert(q->nodes[i].entry.val == val);
    if (new_key >= q->nodes[i].entry.key)
        return -2; /* new_key is not a decrease */

#ifdef PQ_RING
    start = q->start;
    while (i != start) {
        size_t prev = i == 0 ? q->maxsize - 1 : i - 1;
        if (q->nodes[prev].entry.key <= new_key)
            break;
        q->nodes[i].entry = q->nodes[prev].entry;
        q->nodes[q->nodes[i].entry.val].pos = i;
        i = prev;
    }

    q->nodes[i].entry.key = new_key;
    q->nodes[i].entry.val = val;
    q->nodes[val].pos     = i;
    return 0;
#else
    q->nodes[i].entry.key = new_key;
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (q->nodes[parent].entry.key <= new_key)
            break;
        pqueue_swap(q, i, parent);
        i = parent;
    }
    return 0;
#endif
}

/* Peek at the highest priority entry */
struct pqueue_entry*
pqueue_peekmin(struct pqueue *q)
{
#ifdef PQ_RING
    return q->count == 0 ? NULL : &q->nodes[q->start].entry;
#else
    return q->count == 0 ? NULL : &q->nodes[0].entry;
#endif
}

/* Pop the highest priority entry from the queue */
void
pqueue_popmin(struct pqueue *q)
{
#ifdef PQ_RING
    assert(q->count > 0);
    q->count--;
    q->nodes[q->nodes[q->start].entry.val].pos = q->maxsize;
    q->start += 1;
    q->start %= q->maxsize;
#else
    size_t i;
    size_t popped_val;
    assert(q->count > 0);
    popped_val = q->nodes[0].entry.val;
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
    q->nodes[popped_val].pos = q->maxsize;
#endif
}
