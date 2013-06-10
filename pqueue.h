#ifdef PQUEUE_H
#error "double-included pqueue.h"
#endif

#define PQ_RING_H

CONFIG_H;
XINT_H;
XDEF_H;

/*
 * Integer-keyed, integer-valued priority queue.
 *
 * The key and value are actually of type intptr_t, so that pointers
 * may be stored if desired. Lower keys have higher priority.
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
    intptr_t key, val;
};

/* Must be user-visible so that users can allocate fixed-size arrays. */
struct pqueue_node {
    struct pqueue_entry entry;
#ifdef PQ_RING
    /* No PQ_RING-specific members */
#else
#error "heap priority queue not implemented"
#endif
};

struct pqueue {
    struct pqueue_node *nodes;
    size_t              maxsize;
#ifdef PQ_RING
    size_t start;
    size_t count;
#else
#error "heap priority queue not implemented"
#endif
};

/* Initialize a priority queue. It will be able to hold at most maxsize
 * entries. Mem must point to an array of at least maxsize elements.
 * Maxsize must be positive. */
void pqueue_init(struct pqueue *q, size_t maxsize, struct pqueue_node *mem);

/* Add a key/value entry to the priority queue. Returns 0 if the entry
 * was successfully added, and -1 if there was no space. */
int pqueue_add(struct pqueue *q, intptr_t key, intptr_t val);

/* Peek at the minimum-key entry the priority queue. Returns NULL if
 * the queue is empty. The result is valid until pq_pop() is called. */
struct pqueue_entry *pqueue_peekmin(struct pqueue *q);

/* Remove the minimum-key entry from the priority queue, which must
 * not be empty. */
void pqueue_popmin(struct pqueue *q);
