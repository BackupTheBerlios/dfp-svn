/*
 * Cisco Portable Events (CPE)
 *
 * $Id: test-cpe-algorithms.c 141 2007-03-28 08:19:35Z mmolteni $
 */

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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <tap.h>
#include "cpe-algorithms.h"

/*! \bug this should be put in an external script (shell) file,
 *       a lot easier and flexible.
 *
 * generate random data:
 * jot -s "," -r 10
 */
int g_a1[] = {0};
int g_a2[] = {0,0,0,0};
int g_a3[] = {0,1,2,3,4,5};
int g_a4[] = {5,4,3,2,1,0};
int g_a5[] = {1,1,1};
int g_a6[] = {67,26,41,67,18,95,49,72,81,77};
int g_a7[] = {45,66,85,15,15,84,27,83,98,80,88,89,92,21,47,83,60,83,11,61,73,16,12,50,64,8,36,79,21,54,98,3,68,90,97,8,92,87,83,17,73,46,18,91,48,46,96,85,38,32,93,89,64,95,27,33,97,58,10,56,24,90,84,89,80,58,67,80,46,93,9,54,85,38,98,63,24,62,24,55,38,43,17,60,95,66,75,82,90,80,4,54,85,83,65,69,60,87,7,90};
int g_a8[] = {5,-4,3,2,-1,0};

#define FREE(x) do { free(x); x = NULL; } while (0)

static int
less(const void *a, const void *b)
{
    const int *ia = a;
    const int *ib = b;

    if (*ia < *ib) return -1;
    if (*ia > *ib) return 1;
    return 0;
}

static void
test_insert(unsigned int N, int unsorted[])
{
    int *sorted;
    int success;
    unsigned int N2, i;
    cpe_priorityQ *p1, *p2, *head1, *head2;

    sorted = malloc(N * sizeof *sorted);
    memmove(sorted, unsorted, N * sizeof *sorted);
    qsort(sorted, N, sizeof *sorted, less);

    head1 = cpe_priorityQ_create();
    ok(head1 != NULL, "cpe_priorityQ_create");
    head2 = cpe_priorityQ_create();
    ok(head2 != NULL, "cpe_priorityQ_create");

    /* Fill queues "head1", "head2", with values from array "unsorted". */
    for (i = 0; i < N; i++) {
        p1 = cpe_priorityQ_create();
        p2 = cpe_priorityQ_create();
        p1->pq_value = unsorted[i];
        p2->pq_value = unsorted[i];
        cpe_priorityQ_insert(head1, p1);
        cpe_priorityQ_insert(head2, p2);
    }
    ok(cpe_priorityQ_len(head1) == N, "inserted %d elements", N);
    ok(cpe_priorityQ_len(head2) == N, "inserted %d elements", N);

    /* Verify that the queue is correctly sorted. */
    success = 1;
    for (p1 = head1->pq_next, i = 0; p1 != NULL; p1 = p1->pq_next, i++) {
        if (p1->pq_value != sorted[i]) {
            success = 0;
            break;
        }
    }
    ok(success, "correct order");
    ok(i == N, "correct size (%d)", i);

    /* Verify find and remove max */
    for (i = 0; i < N; i++) {
        cpe_priorityQ *max_find, *max_del;
        unsigned int len;

        max_find = cpe_priorityQ_find_max(head1);
        ok(max_find->pq_value == sorted[i], "find_max (%d)",
            max_find->pq_value);
        max_del = cpe_priorityQ_remove_max(head1);
        ok(max_del->pq_value == max_find->pq_value, "remove_max find (%d)",
            max_del->pq_value);
        len = cpe_priorityQ_len(head1);
        ok(len == (N - 1) - i, "remove_max remove (len %d)", len);
    }
    ok(cpe_priorityQ_len(head1) == 0, "empty queue");

    /* Verify queue destroy */
    N2 = cpe_priorityQ_queue_destroy(head2);
    ok(N == N2, "cpe_priorityQ_queue_destroy correct count (%d/%d)", N, N2);
    ok(cpe_priorityQ_len(head2) == 0, "empty queue");

    /*! \bug replace with proper destructor */
    FREE(head1);
    FREE(head2);

    FREE(sorted);
}

static void
test_remove(unsigned int N, int unsorted[])
{
    unsigned int i;
    cpe_priorityQ *p1, *head1, *q1, *q2;
    cpe_priorityQ **pointers;

    pointers = calloc(N, sizeof *pointers);
    assert(pointers);

    head1 = cpe_priorityQ_create();
    ok(head1 != NULL, "cpe_priorityQ_create");

    ok(cpe_priorityQ_remove(head1, head1) == -1,
        "remove element with empty queue");

    /* Fill queue "head1" with values from array "unsorted". */
    for (i = 0; i < N; i++) {
        p1 = cpe_priorityQ_create();
        p1->pq_value = unsorted[i];
        cpe_priorityQ_insert(head1, p1);
        pointers[i] = p1;
    }
    ok(cpe_priorityQ_len(head1) == N, "inserted %d elements", N);

    q1 = NULL;
    q2 = (cpe_priorityQ *) 0xcafefade;
    //ok(cpe_priorityQ_remove(head1, q1) == -1, "remove element fail");
    ok(cpe_priorityQ_remove(head1, q2) == -1, "remove element fail");

    for (i = 0; i < N; i++) {
        ok(cpe_priorityQ_remove(head1, pointers[i]) == 1, "remove element ok");
        FREE(pointers[i]);
    }
    ok(cpe_priorityQ_len(head1) == 0, "empty queue");

    FREE(pointers);
}

static void
test_sentinel(void)
{
    int64_t       a1[] = {CPE_PRIORITYQ_AT_THE_END, 1, 15};
    int64_t       a2[] = {1, 15, CPE_PRIORITYQ_AT_THE_END};
    cpe_priorityQ *p1, *head1;
    unsigned int  i, N, N2;
    int success;

    N = sizeof a1 / sizeof a1[0];
    head1 = cpe_priorityQ_create();
    ok(head1 != NULL, "cpe_priorityQ_create");
    for (i = 0; i < N; i++) {
        p1 = cpe_priorityQ_create();
        p1->pq_value = a1[i];
        cpe_priorityQ_insert(head1, p1);
    }
    ok(cpe_priorityQ_len(head1) == N, "inserted %d elements", N);

    /* Verify that the queue is correctly sorted. */
    success = 1;
    for (p1 = head1->pq_next, i = 0; p1 != NULL; p1 = p1->pq_next, i++) {
        if (p1->pq_value != a2[i]) {
            success = 0;
            break;
        }
    }
    ok(success, "correct order");
    ok(i == N, "correct size (%d)", i);

    /* Verify queue destroy */
    N2 = cpe_priorityQ_queue_destroy(head1);
    ok(N == N2, "cpe_priorityQ_queue_destroy correct count (%d/%d)", N, N2);
    ok(cpe_priorityQ_len(head1) == 0, "empty queue");

    /*! \bug replace with proper destructor */
    FREE(head1);
}

int
main(void)
{
    cpe_priorityQ *head;
    unsigned int N;

    plan_tests(502);

    head = cpe_priorityQ_create();
    ok(head != NULL, "cpe_priorityQ_create");
    ok(cpe_priorityQ_len(head) == 0, "len of empty queue");
    ok(cpe_priorityQ_find_max(head) == NULL, "find max of empty queue");
    ok(cpe_priorityQ_remove_max(head) == NULL, "remove max of empty queue");
    N = cpe_priorityQ_queue_destroy(head);
    ok(N == 0, "destroy of empty queue, len %d", N);

    FREE(head);

#define ARGS(x) sizeof x / sizeof x[0], x

    test_remove(ARGS(g_a4));

    test_insert(ARGS(g_a1));
    test_insert(ARGS(g_a2));
    test_insert(ARGS(g_a3));
    test_insert(ARGS(g_a4));
    test_insert(ARGS(g_a5));
    test_insert(ARGS(g_a6));
    test_insert(ARGS(g_a7));
    test_insert(ARGS(g_a8));

#undef ARGS

    test_sentinel();

    return exit_status();
}
