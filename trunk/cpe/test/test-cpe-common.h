/*
 * Cisco Portable Events (CPE)
 *
 * $Id$
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

#ifndef TEST_CPE_COMMON_INCLUDED
#define TEST_CPE_COMMON_INCLUDED

#include <tap.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <apr_getopt.h>
#include "cpe.h"
#include "cpe-logging.h"
#include "cpe-network.h"

#define SERVER_ADDR     "127.0.0.1"
#define SERVER_PORT     12345
/* Note: on my machine, a tolerance of 3 msec is always respected.
 * On the other hand, when run from prove(1), many tests fail. Waiting for
 * an analysis of the problem, we just keep a generous tolerance.
 */
#define TIMER_TOL_MSEC  10          /* tolerance in msec */

#define CBDATA_INIT2(basename, Tusec, timenow, duration) \
    basename.last_seen = timenow;                        \
    basename.timeout = (Tusec);                          \
    basename.expected_count = (duration) / (Tusec)

typedef struct test_timer_ctx_ {
    cpe_io_buf      **t_iobufs; /* will become array of pointers to cpe_io_buf */
    int               t_index;
    unsigned int      t_nelems;
    uint32_t          t_csum;
    cpe_network_ctx  *t_nc;
} test_timer_ctx_t;



struct timer_data {
    apr_time_t  last_seen;
    int         count;
    int         expected_count;
    int         one_shot;
    apr_time_t  timeout;
    char       *msg;
};

struct conf_t {
    int         co_debug;
    int         co_loop_duration;
    int         co_listen_port;
    apr_pool_t *co_pool;
};
typedef struct conf_t conf_t;

extern conf_t g_conf;

apr_status_t
parse_args(conf_t *conf, int argc, const char *const *argv);
apr_status_t
timer_init(test_timer_ctx_t **ctx, cpe_network_ctx *nc, apr_pool_t *pool,
    int nelems, int mybufsize);
int
find_next_avail_buffer(test_timer_ctx_t *ctx);
apr_status_t
test_timer_cb(void *arg, apr_pollfd_t *pfd, cpe_event *e);
void
test_network_io1(conf_t *conf, apr_time_t *runtime,
    cpe_callback_t server_cb, cpe_network_ctx *server_ctx, apr_int16_t s_flags,
    cpe_callback_t server_one_shot_cb, void *server_one_shot_ctx,
    cpe_callback_t client_cb, cpe_network_ctx *client_ctx, apr_int16_t c_flags,
    cpe_callback_t client_one_shot_cb, void *client_one_shot_ctx);
apr_status_t
test_init(conf_t *conf);
apr_status_t
test_run(conf_t *conf);


#endif /* TEST_CPE_COMMON_INCLUDED */
