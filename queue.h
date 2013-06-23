#ifdef QUEUE_H
#error "double-included queue.h"
#endif

#define QUEUE_H

XBOOL_H;

struct queue {
    void *mem;
    int   eltsize;
    int   cap;
    int   wr;
    int   rd;
    int   size;
    int   count;
};

void q_init(struct queue *q, void *mem, int eltsize, int maxsize);
bool q_dequeue(struct queue *q, void *buf_out);
int  q_enqueue(struct queue *q, const void *buf_in);
int  q_size(struct queue *q);
