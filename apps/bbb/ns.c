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
/*
* Some of this code is pulled from TI's BeagleBone Black StarterWare
* Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
*
*  Redistribution and use in source and binary forms, with or without 
*  modification, are permitted provided that the following conditions 
*  are met:
*
*    Redistributions of source code must retain the above copyright 
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the 
*    documentation and/or other materials provided with the   
*    distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include "u_tid.h"
#include "ns.h"

#include "xbool.h"
#include "xdef.h"
#include "xstring.h"
#include "xassert.h"
#include "xmemcpy.h"
#include "u_syscall.h"
#include "config.h"

#define MAX_NAMES       MAX_TASKS
#define NS_TID_UNKNOWN  (-1) /* sentinel tid for unregistered names */

/* Message format. */
enum {
    NS_MSG_REGISTER,
    NS_MSG_WHOIS
};

struct ns_msg {
    int  type;
    char name[NS_NAME_MAXLEN]; /* NUL-terminated */
};

/* Replies to a registration message. (WhoIs always succeeds) */
enum {
    NS_RPLY_SUCCESS = 0,
    NS_RPLY_NOSPACE = -3
};

/* Name record. This keeps a TID if any, and a queue of waiting tasks. */
struct nsrec {
    /* Which name is this record for?
     * There should only be one record per name */
    char name[NS_NAME_MAXLEN];

    /* The TID of the registered task, or NS_TID_UNKNOWN. */
    tid_t tid;

    /* If TID is unknown, stack of tasks waiting for a registration
     * in this name.
     *
     * A stack is used because order is unimportant---all waiting tasks
     * are replied to before any further requests are processed. */
    struct nswait *waitq;
};

/* A node in a stack of waiting tasks. */
struct nswait {
    tid_t          tid;
    struct nswait *next;
};

/* Name server database/state. */
struct nsdb {
    int          count;               /* record count, up to MAX_NAMES */
    struct nsrec records[MAX_NAMES];  /* [0..count-1] are all records  */

    /* Wait queue nodes allocation. */
    struct nswait  wait[MAX_TASKS]; /* never need more than 1 per task */
    struct nswait *wait_free;       /* free list (stack) */
};

/* Initialize name server state - no name records, all nswait nodes free. */
static void ns_init(struct nsdb*);

/* Implementation of RegisterAs() and WhoIs().  */
static void ns_register(struct nsdb*, const char[NS_NAME_MAXLEN], tid_t);
static void ns_whois(struct nsdb*, const char[NS_NAME_MAXLEN], tid_t);
static void ns_reply(tid_t tid, int rply);

/* Name record search. Second variant creates. Uses linear search. */
static struct nsrec *ns_find(struct nsdb*, const char[NS_NAME_MAXLEN]);
static struct nsrec *ns_find_create(struct nsdb*, const char[NS_NAME_MAXLEN], tid_t tid);

/* nswait stack operations. */
static void nswait_push(struct nsdb*, struct nswait**, tid_t);
static bool nswait_trypop(struct nsdb*, struct nswait**, tid_t*);
static void nswait_push_node(struct nswait**, struct nswait*);
static bool nswait_trypop_node(struct nswait**, struct nswait **out);
static struct nswait *nswait_alloc_node(struct nsdb*);
static void nswait_free_node(struct nsdb*, struct nswait*);

/* Name server entry point */
void
ns_main(void)
{
    struct nsdb   db;
    struct ns_msg msg;
    tid_t         sender;
    int           msglen;

    ns_init(&db);
    for (;;) {
        msglen = Receive(&sender, &msg, sizeof (msg));
        assertv(msglen, msglen == sizeof (msg));
        switch (msg.type) {
        case NS_MSG_REGISTER:
            ns_register(&db, msg.name, sender);
            break;
        case NS_MSG_WHOIS:
            ns_whois(&db, msg.name, sender);
            break;
        default:
            assert(false);
        }
    }
}

static void
ns_init(struct nsdb *db)
{
    int i;
    db->count     = 0;
    db->wait_free = NULL;
    for (i = MAX_TASKS - 1; i >= 0; i--)
        nswait_free_node(db, &db->wait[i]);
}

static void
ns_register(struct nsdb *db, const char name[NS_NAME_MAXLEN], tid_t tid)
{
    struct nsrec *rec;
    tid_t whois_client;

    rec = ns_find_create(db, name, tid);
    if (rec == NULL) {
        ns_reply(tid, NS_RPLY_NOSPACE);
        return;
    }

    rec->tid = tid;
    ns_reply(tid, NS_RPLY_SUCCESS);
    while (nswait_trypop(db, &rec->waitq, &whois_client))
        ns_reply(whois_client, tid);
}

static void
ns_whois(struct nsdb *db, const char name[NS_NAME_MAXLEN], tid_t tid)
{
    struct nsrec *rec;
    rec = ns_find_create(db, name, -1);
    if (rec == NULL)
        return; /* block until the name is registered, which is forever */

    if (rec->tid >= 0) {
        /* Task has been registered, reply with its TID */
        ns_reply(tid, rec->tid);
    } else {
        /* Placeholder with wait queue. Add self to queue. */
        assert(rec->tid == -1);
        nswait_push(db, &rec->waitq, tid);
    }
}

static void
ns_reply(tid_t tid, int rply)
{
    int rc = Reply(tid, &rply, sizeof (rply));
    assertv(rc, rc == 0);
}

static struct nsrec*
ns_find(struct nsdb *db, const char name[NS_NAME_MAXLEN])
{
    int i;
    for (i = 0; i < db->count; i++) {
        struct nsrec *rec = &db->records[i];
        if (strcmp(name, rec->name) == 0)
            return rec;
    }
    return NULL;
}

static struct nsrec*
ns_find_create(struct nsdb *db, const char name[NS_NAME_MAXLEN], tid_t tid)
{
    struct nsrec *rec = ns_find(db, name);
    if (rec != NULL)
        return rec;
    else if (db->count == MAX_NAMES)
        return NULL;

    rec = &db->records[db->count++];
    memcpy(rec->name, name, NS_NAME_MAXLEN);
    rec->tid   = tid;
    rec->waitq = NULL;
    return rec;
}

static void
nswait_push(struct nsdb *db, struct nswait **stack, tid_t tid)
{
    struct nswait *node = nswait_alloc_node(db);
    assert(node != NULL); /* can't use too many nodes */
    node->tid = tid;
    nswait_push_node(stack, node);
}

static bool
nswait_trypop(struct nsdb *db, struct nswait **stack, tid_t *tid_out)
{
    struct nswait *node;
    if (!nswait_trypop_node(stack, &node))
        return false;

    *tid_out = node->tid;
    nswait_free_node(db, node);
    return true;
}

static void
nswait_push_node(struct nswait **stack, struct nswait *node)
{
    node->next = *stack;
    *stack     = node;
}

static bool
nswait_trypop_node(struct nswait **stack, struct nswait **node_out)
{
    if (*stack == NULL)
        return false;

    *node_out = *stack;
    *stack = (*stack)->next;
    (*node_out)->next = NULL;
    return true;
}

static struct nswait*
nswait_alloc_node(struct nsdb *db)
{
    struct nswait *node;
    return nswait_trypop_node(&db->wait_free, &node) ? node : NULL;
}

static void
nswait_free_node(struct nsdb *db, struct nswait *node)
{
    nswait_push_node(&db->wait_free, node);
}

int
RegisterAs(const char *name)
{
    struct ns_msg msg;
    int           rc, rplylen, namesize;
    namesize = strnlen(name, NS_NAME_MAXLEN) + 1;
    assertv(namesize, namesize <= NS_NAME_MAXLEN);
    msg.type = NS_MSG_REGISTER;
    memcpy(msg.name, name, namesize);
    rplylen = Send(NS_TID, &msg, sizeof (msg), &rc, sizeof (rc));
    assertv(rplylen, rplylen == sizeof (rc));
    return rc;
}

tid_t
WhoIs(const char *name)
{
    struct ns_msg msg;
    int           tid, rplylen, namesize;
    namesize = strnlen(name, NS_NAME_MAXLEN) + 1;
    assertv(namesize, namesize <= NS_NAME_MAXLEN);
    msg.type = NS_MSG_WHOIS;
    memcpy(msg.name, name, namesize);
    rplylen = Send(NS_TID, &msg, sizeof (msg), &tid, sizeof (tid));
    assertv(rplylen, rplylen == sizeof (tid));
    return tid;
}
