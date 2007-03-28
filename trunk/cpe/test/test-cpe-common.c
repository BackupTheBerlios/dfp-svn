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

#include "test-cpe-common.h"

conf_t g_conf;


apr_status_t
timer_init(test_timer_ctx_t **ctx, cpe_network_ctx *nc, apr_pool_t *pool,
    int nelems, int mybufsize)
{
    test_timer_ctx_t  *t1;
    uint               i;
    apr_status_t       rv;

    cpe_log(CPE_DEB, "%s", "enter");
    *ctx = NULL;
    CHECK_NULL(t1, apr_pcalloc(pool, sizeof(test_timer_ctx_t)));
    t1->t_nc = nc;
    t1->t_nelems = nelems;
    CHECK_NULL(t1->t_iobufs,
        apr_pcalloc(pool, t1->t_nelems * sizeof(*t1->t_iobufs)));
    for (i = 0; i < t1->t_nelems; i++) {
        CHECK(cpe_iobuf_create(&t1->t_iobufs[i], mybufsize, pool));
    }
    *ctx = t1;
    return APR_SUCCESS;
}


int
find_next_avail_buffer(test_timer_ctx_t *ctx)
{
    unsigned int i;
    cpe_io_buf   *iobuf;

    for (i = 0; i < ctx->t_nelems; i++) {
        ctx->t_index++;
        ctx->t_index %= ctx->t_nelems;
        iobuf = ctx->t_iobufs[ctx->t_index];
        if (iobuf->inqueue == 0) {
            break;
        }
    }
    return iobuf->inqueue == 0;
}


apr_status_t
parse_args(conf_t *conf, int argc, const char *const *argv)
{
    apr_status_t  rv;
    apr_pool_t   *pool;
    static const apr_getopt_option_t options[] = {
        /* long-option, short-option, has-arg flag, description */
        { "debug",   'd', TRUE,  "debug level"                     },
        { "port",    'p', TRUE,  "listening port"                  },
        { "timeout", 't', TRUE,  "timeout for CPE main loop [sec]" },
        { NULL,       0,  0,     NULL                              } /* end */
    };
    apr_getopt_t *opt;
    int och;
    const char *oarg;
    int i;

    apr_pool_create(&pool, NULL);
    apr_getopt_init(&opt, pool, argc, argv);

    while ((rv = apr_getopt_long(opt, options, &och, &oarg)) == APR_SUCCESS) {
        switch (och) {
        case 'd':
            conf->co_debug = atoi(oarg);
            cpe_log_init(conf->co_debug);
            break;
        case 'p':
            conf->co_listen_port = atoi(oarg);
            break;
        case 't':
            conf->co_loop_duration = apr_time_from_sec(atoi(oarg));
            break;
        }
    }
    if (rv == APR_BADCH) {
        printf("usage: %s [opts]\n", argv[0]);
        for (i = 0; options[i].name != NULL; i++) {
            printf("-%c %s\n", options[i].optch, options[i].description);
        }
    }
    apr_pool_destroy(pool);
    if (rv == APR_EOF) {
        return APR_SUCCESS;
    }
    return rv;
}


apr_status_t
test_timer_cb(void *arg, apr_pollfd_t *pfd, cpe_event *e)
{
    struct timer_data *cb;
    apr_time_t         time_now;
    apr_time_t         measured_timeout;
    apr_time_t         delta, abs_delta;

    pfd = NULL;
    cb = (struct timer_data *) arg;
    if (! cb->one_shot) {
        cpe_event_add(e);
    }
    time_now = apr_time_now();
    measured_timeout = time_now - cb->last_seen;
    delta = measured_timeout - cb->timeout;
    if (delta < 0) {
        abs_delta = -delta;
    } else {
        abs_delta = delta;
    }

    ok(apr_time_as_msec(abs_delta) < TIMER_TOL_MSEC, "timer in tol "
        "(%s: now %lld ms, to %4lld ms, m_to %4lld ms, diff %lld ms)",
        cb->msg, apr_time_as_msec(time_now),
        apr_time_as_msec(cb->timeout), apr_time_as_msec(measured_timeout),
        apr_time_as_msec(delta));
    cb->last_seen = time_now;
    cb->count++;

    return APR_SUCCESS;
}


/* NOTE in this test the timeout is performed by apr_pollset_poll().
 *
 */
void
test_network_io1(conf_t *conf, apr_time_t *runtime,
    cpe_callback_t server_cb, cpe_network_ctx *server_ctx, apr_int16_t s_flags,
    cpe_callback_t server_one_shot_cb, void *server_one_shot_ctx,
    cpe_callback_t client_cb, cpe_network_ctx *client_ctx, apr_int16_t c_flags,
    cpe_callback_t client_one_shot_cb, void *client_one_shot_ctx)
{
    apr_socket_t     *lsock, *csock;
    apr_sockaddr_t   *sockaddr;
    apr_status_t      rv;
    int               count, backlog, max_peers;
    apr_time_t        time_now;

    /*
     * Setup write/read to a TCP socket on localhost, server side.
     */
    backlog = 1;
    max_peers = 10;
    rv = cpe_socket_server_create(&lsock, &sockaddr, SERVER_ADDR,
        conf->co_listen_port, backlog, conf->co_pool);
    ok(rv == APR_SUCCESS, "cpe_socket_server_create");

    rv = cpe_socket_after_accept(lsock, server_cb, server_ctx, s_flags, NULL,
        max_peers, server_one_shot_cb, server_one_shot_ctx, conf->co_pool);
    ok(rv == APR_SUCCESS, "cpe_socket_after_accept");

    /*
     * Setup write/read to a TCP socket on localhost, client side.
     */
     /* WARNING on localhost, we probably cannot test a non blocking connect,
     * since the connect will immediately return successful.
     * To be sure we test it, we need a delay in the network path.
     */
    rv = cpe_socket_client_create(&csock, &sockaddr, SERVER_ADDR,
        conf->co_listen_port, conf->co_pool);
    ok(rv == APR_SUCCESS, "cpe_socket_client_create");

    rv = cpe_socket_after_connect(csock, sockaddr, 0, client_cb,
        client_ctx, c_flags, client_one_shot_cb, client_one_shot_ctx,
        conf->co_pool);
    ok(rv == APR_SUCCESS, "cpe_socket_after_connect");

    //diag("listening on %s %d for %d s", SERVER_ADDR, conf->co_listen_port,
    //    apr_time_sec(conf->co_loop_duration));
    count = cpe_events_in_system();
    //diag("before entering main loop, events in system: %d", count);
    time_now = apr_time_now();
    ok(cpe_main_loop(conf->co_loop_duration) == APR_SUCCESS,
        "event main loop");
    *runtime = apr_time_now() - time_now;
    cpe_log(CPE_DEB, "Elapsed time %" APR_TIME_T_FMT " ms", apr_time_as_msec(*runtime));

    count = cpe_events_in_system();
    ok(count == 0, "after main loop, event system empty (%d)", count);
    // XXX TRANSFORM THOSE TWO INTO A MORE MEANINGFUL TEST
    ok(server_ctx->nc_count > 0, "server cb invoked at least once (%d)",
        server_ctx->nc_count);
    ok(client_ctx->nc_count > 0, "client cb invoked at least once (%d)",
        client_ctx->nc_count);
}


int
main(int argc, const char *const *argv)
{
    apr_status_t rv;

    CHECK(apr_app_initialize(&argc, &argv, NULL));
    test_init(&g_conf);
    CHECK(parse_args(&g_conf, argc, argv));
    ok(cpe_system_init(CPE_NUM_EVENTS_DEFAULT) == APR_SUCCESS,
        "cpe_system_init");
    test_run(&g_conf);

    return exit_status();
}
