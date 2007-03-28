/* $Id$ */

/*
The Cisco-style BSD License
Copyright (c) 2007, Cisco Systems, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

Neither the name of Cisco Systems, Inc. nor the names of its contributors may
be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Loosely based on code from the book "Algorithms in C",
 * which requires the following credit:
 *
 *   This code is from "Algorithms in C, Third Edition,"
 *   by Robert Sedgewick, Addison-Wesley, 1998.
 *
 * Original code Copyright 1998 by Addison-Wesley Publishing Company, Inc.
 */

#include "cpe-algorithms.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


cpe_priorityQ *
cpe_priorityQ_create(void)
{
    cpe_priorityQ *p;
    /*!
     * \todo replace with a apr pool alloc
     */
    p = calloc(1, sizeof *p);
    return p;
}

/*
 * keep a sorted list on pq_value, from min to max. Allow duplicate pq_value.
 * \todo replace with a proper implementation of priority queue
 *       (heap or RB tree)
 * \todo "u" is useless here, refactor code with tests to remove "u"
 */
void
cpe_priorityQ_insert(cpe_priorityQ *head, cpe_priorityQ *node)
{
    cpe_priorityQ *p, *u;

    assert(head != NULL);
    assert(node != NULL);
    for (p = head; p->pq_next != NULL; p = u) {
        u = p->pq_next;
        if (node->pq_value < u->pq_value) {
            break;
        }
    }
    /* Don't allow duplicate "nodes", they are a bug. */
    assert(p != node);
    node->pq_next = p->pq_next;
    p->pq_next = node;
}

/*!
 * This function doesn't deallocate the memory pointed to by "node" because
 * cpe_priorityQ allows for subclassing. It is responsability of the caller.
 */
int
cpe_priorityQ_remove(cpe_priorityQ *head, cpe_priorityQ *node)
{
    cpe_priorityQ *p, *u;

    assert(head != NULL);
    assert(node != NULL);

    for (p = head; p != NULL; p = u) {
        u = p->pq_next;
        if (u == node) {
            p->pq_next = u->pq_next;
            u->pq_next = NULL;
            return 1;
        }
    }
    return -1;
}

cpe_priorityQ *
cpe_priorityQ_find_max(cpe_priorityQ *head)
{
    assert(head != NULL);
    return head->pq_next;
}

cpe_priorityQ *
cpe_priorityQ_remove_max(cpe_priorityQ *head)
{
    cpe_priorityQ *p;

    assert(head != NULL);
    p = head->pq_next;
    if (p != NULL) {
        head->pq_next = p->pq_next;
        p->pq_next = NULL;
    }
    return p;
}

u_int
cpe_priorityQ_queue_destroy(cpe_priorityQ *head)
{
    cpe_priorityQ *p, *u;
    u_int count = 0;

    assert(head != NULL);
    for (p = head->pq_next; p != NULL; p = u) {
        u = p->pq_next;
        free(p->pq_data);
        free(p);
        count++;
    }
    head->pq_next = NULL;
    return count;
}

/*!
 * \todo add a function pointer to print pq_item
 */
void
cpe_priorityQ_print(cpe_priorityQ *head)
{
    cpe_priorityQ *p;

    assert(head != NULL);
    for (p = head->pq_next; p != NULL; p = p->pq_next) {
        printf("%lld ", p->pq_value);
    }
}

u_int
cpe_priorityQ_len(cpe_priorityQ *head)
{
    cpe_priorityQ *p;
    u_int count = 0;

    assert(head != NULL);
    for (p = head->pq_next; p != NULL; p = p->pq_next) {
        count++;
    }
    return count;
}
