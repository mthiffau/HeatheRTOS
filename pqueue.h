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

#ifndef PQUEUE_H
#define PQ_RING_H

#include "config.h"
#include "xint.h"
#include "xdef.h"

/*
 * Integer-keyed, integer-valued priority queue.
 *
 * For a queue with maximum size N, only the values 0 through N-1
 * can be stored, and no duplicate values are allowed.
 *
 * The key is actually of type intptr_t, so that pointers
 * may be used as keys if desired. Lower keys have higher priority.
 *
 * Exactly one of PQ_RING and PQ_HEAP must be #defined in order to
 * select implementation.
 */

#if !defined(PQ_RING) && !defined(PQ_HEAP)
#error "must define either PQ_RING or PQ_HEAP"
#endif

#if defined(PQ_RING) && defined(PQ_HEAP)
#error "must not befine both PQ_RING and PQ_HEAP"
#endif

struct pqueue_entry {
    intptr_t key;
    size_t   val;
};

/* Must be user-visible so that users can allocate fixed-size arrays.
 *
 * Each implementation employs an array of maxsize of these.
 * The entries are placed differently dependending on implementation,
 * but nodes[i] always has the position for value i. */
struct pqueue_node {
    struct pqueue_entry entry;
    size_t              pos;
#ifdef PQ_RING
    /* No PQ_RING-specific members */
#else
    /* No PQ_HEAP-specific members */
#endif
};

struct pqueue {
    struct pqueue_node *nodes;
    size_t              maxsize;
    size_t              count;
#ifdef PQ_RING
    size_t start;
#else
    /* No PQ_HEAP-specific members */
#endif
};

/* Initialize a priority queue. It will be able to hold at most maxsize
 * entries. Mem must point to an array of at least maxsize elements.
 * Maxsize must be positive. */
void pqueue_init(struct pqueue *q, size_t maxsize, struct pqueue_node *mem);

/* Add a key/value entry to the priority queue. Returns:
 *    0 if the entry was successfully added,
 *   -1 if the value was out of range,
 *   -2 if if the value was already in the queue. */
int pqueue_add(struct pqueue *q, size_t val, intptr_t key);

/* Decrease the key of a value in the priority queue. Returns:
 *    0 if the value's key was successfully decreased,
 *   -1 if the value is not in the queue, or
 *   -2 if the new key is not less than the previous key. */
int pqueue_decreasekey(struct pqueue *q, size_t val, intptr_t new_key);

/* Peek at the minimum-key entry the priority queue. Returns NULL if
 * the queue is empty. The result is valid until pq_pop() is called. */
struct pqueue_entry *pqueue_peekmin(struct pqueue *q);

/* Remove the minimum-key entry from the priority queue, which must
 * not be empty. */
void pqueue_popmin(struct pqueue *q);

#endif
