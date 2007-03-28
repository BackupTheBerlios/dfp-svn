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

#ifndef CPE_ALGORITHMS_INCLUDED
#define CPE_ALGORITHMS_INCLUDED

#include <inttypes.h>
#include <sys/types.h>
#include <limits.h>

/*
 * Used to "subclass" struct cpe_priorityQ.
 * XXX yes I know, a 64-bit key. This is because APR uses int64_t for time
 * values.
 */
#define CPE_PRIORITYQ_HEADER       \
    struct cpe_priorityQ *pq_next; \
    void                 *pq_data; \
    int64_t              pq_value;

struct cpe_priorityQ {
    CPE_PRIORITYQ_HEADER
};

typedef struct cpe_priorityQ cpe_priorityQ;

/* Linux + gcc */
#ifndef LLONG_MAX
#define LLONG_MAX   LONG_LONG_MAX
#endif
/* Force insert at the end.
 * Do not use unless you know very well what you are doing.
 */
#define CPE_PRIORITYQ_AT_THE_END LLONG_MAX


cpe_priorityQ *cpe_priorityQ_create(void);
void           cpe_priorityQ_insert(cpe_priorityQ *head, cpe_priorityQ *node);
u_int          cpe_priorityQ_len(cpe_priorityQ *head);
u_int          cpe_priorityQ_queue_destroy(cpe_priorityQ *head);
void           cpe_priorityQ_print(cpe_priorityQ *head);
cpe_priorityQ *cpe_priorityQ_find_max(cpe_priorityQ *head);
cpe_priorityQ *cpe_priorityQ_remove_max(cpe_priorityQ *head);
int            cpe_priorityQ_remove(cpe_priorityQ *head, cpe_priorityQ *node);


#endif /* CPE_ALGORITHMS_INCLUDED */
