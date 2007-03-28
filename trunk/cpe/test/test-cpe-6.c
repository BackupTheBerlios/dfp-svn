/*
 * Cisco Portable Events (CPE)
 * $Id: test-cpe-6.c 136 2007-03-27 12:27:01Z mmolteni $
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

#include "test-cpe-common.h"

/* a dummy callback, will peg the CPU usage to 100%. Use only if you know
 * what you are doing.
 */
static apr_status_t
fdesc_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    cpe_network_ctx *ctx = (cpe_network_ctx *) context;

    pfd = NULL;
    cpe_event_add(e);

    /* pass tests in test-cpe-common */
    ctx->nc_count++;

    apr_sleep(cpe_time_from_msec(5));
    return APR_SUCCESS;
}

/* this is needed to avoid waiting forever in the main loop, since we are
 * calling the main loop with max_wait = 0.
 */
static apr_status_t
timer_cb(void *ctx, apr_pollfd_t *pfd, cpe_event *e)
{
    ctx = NULL;
    pfd = NULL;
    e = NULL;

    cpe_log(CPE_DEB, "%s", "timer_cb: exiting from main loop");
    cpe_main_loop_terminate();
    return APR_SUCCESS;
}

apr_status_t
test_init(conf_t *conf)
{
    apr_status_t rv;

    conf->co_debug = CPE_INFO;
    conf->co_listen_port = SERVER_PORT;
    conf->co_loop_duration = 0; /* wait forever */
    CHECK(apr_pool_create(&conf->co_pool, NULL));

    plan_tests(12);

    return APR_SUCCESS;
}

/* test that a timeout of 0 through cpe_pollset_poll/apr_pollset_poll
 * as opposed to cpe_pollset_poll/apr_wait works as expected
 */
apr_status_t
test_run(conf_t *conf)
{
    cpe_network_ctx  server_ctx, client_ctx;
    apr_int16_t      s_flags, c_flags;
    cpe_event       *event;
    apr_time_t       timeout;
    apr_time_t       runtime;

    memset(&server_ctx, 0, sizeof server_ctx);
    memset(&client_ctx, 0, sizeof client_ctx);
    s_flags = c_flags = APR_POLLIN | APR_POLLOUT;

    /* this is the real test timeout */
    timeout = cpe_time_from_msec(200);
    event = cpe_event_timer_create(timeout, timer_cb, NULL);
    ok(event != NULL, "create timer event");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event");

    test_network_io1(conf, &runtime,
        fdesc_cb, &server_ctx, s_flags, NULL, NULL,
        fdesc_cb, &client_ctx, c_flags, NULL, NULL);

    // As is, this is a bit too coarse grained.
    ok(runtime > conf->co_loop_duration,
        "runtime > loop duration (delta %lld ms)",
        apr_time_as_msec(runtime - conf->co_loop_duration));

    return APR_SUCCESS;
}
