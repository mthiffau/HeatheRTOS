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
