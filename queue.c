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

#include "xbool.h"
#include "queue.h"

#include "xdef.h"
#include "xmemcpy.h"

#include "xassert.h"

/* Initialize a queue */
void
q_init(struct queue *q, void *mem, int eltsize, int memsize)
{
    q->mem     = mem;
    q->eltsize = eltsize;
    q->cap     = (memsize / eltsize) * eltsize;
    q->wr      = 0;
    q->rd      = 0;
    q->size    = 0;
    q->count   = 0;
}

/* Remove an item from the queue */
bool
q_dequeue(struct queue *q, void *buf_out)
{
    if (q->size == 0)
        return false;

    memcpy(buf_out, q->mem + q->rd, q->eltsize);
    q->rd   += q->eltsize;
    q->rd   %= q->cap;
    q->size -= q->eltsize;
    q->count--;
    return true;
}

/* Add an item to the queue */
int
q_enqueue(struct queue *q, const void *buf_in)
{
    if (q->size == q->cap)
        return -1;

    assert(q->size <= q->cap - q->eltsize);
    memcpy(q->mem + q->wr, buf_in, q->eltsize);
    q->wr   += q->eltsize;
    q->wr   %= q->cap;
    q->size += q->eltsize;
    q->count++;
    return 0;
}

/* Returns how many items are in a queue */
int
q_size(struct queue *q)
{
    return q->count;
}
