#ifndef QUEUE_H
#define QUEUE_H

#include "xbool.h"

struct queue {
    void *mem;
    int   eltsize;
    int   cap;
    int   wr;
    int   rd;
    int   size;
    int   count;
};

/* Initialize a queue */
void q_init(struct queue *q, void *mem, int eltsize, int maxsize);
/* Remove an item from the queue */
bool q_dequeue(struct queue *q, void *buf_out);
/* Add an item to the queue */
int  q_enqueue(struct queue *q, const void *buf_in);
/* Returns how many items are in a queue */
int  q_size(struct queue *q);

#endif
