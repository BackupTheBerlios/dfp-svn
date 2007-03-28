/* $Id$
 * Test if a cleanup function is called on socket close.
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

#include "stdio.h"

#include "cpe.h"
#include "cpe-logging.h"
#include "apr_general.h"

struct ctx_ {
    int a;
    int b;
} ctx;

static apr_status_t
cleanup_cb(void *q)
{
    printf("%s: enter, q: %p\n", __FUNCTION__, q);
    return APR_SUCCESS;
}

int main(void)
{
    apr_status_t   rv;
    apr_pool_t    *pool;
    apr_socket_t  *sock;

    CHECK(apr_initialize());
    CHECK(apr_pool_create(&pool, NULL));
    CHECK(apr_socket_create(&sock, APR_INET, SOCK_STREAM, APR_PROTO_TCP, pool));
    ctx.a = 1; ctx.b = 2;
    CHECK(apr_socket_data_set(sock, &ctx, "dummy", cleanup_cb));
    printf("closing socket\n");
    CHECK(apr_socket_close(sock));
    printf("destroying pool\n");
    apr_pool_destroy(pool);

    return 0;
}
