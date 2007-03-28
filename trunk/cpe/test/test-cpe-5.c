/*
 * Cisco Portable Events (CPE)
 * $Id: test-cpe-5.c 136 2007-03-27 12:27:01Z mmolteni $
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

#define MYBUFSIZE   10 * ONE_SI_MEGA

static int g_expected_total;


/* Send the buffer in chunks.
 */
static apr_status_t
server_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    cpe_network_ctx *ctx = (cpe_network_ctx *) context;
    cpe_io_buf      *sendbuf = &ctx->nc_send;
    apr_status_t     rv;
    apr_size_t       howmany;

    cpe_event_add(e);
    ctx->nc_count++;
    assert((pfd->rtnevents & APR_POLLIN) == 0);

    /* We use a small odd size to stress CPE. */
    howmany = 497;
    rv = cpe_send(pfd->desc.s, sendbuf, &howmany);
    /* XXX what to do with rv ? */
    assert(sendbuf->buf_offset <= sendbuf->buf_len);
    if (sendbuf->buf_offset == sendbuf->buf_len) {
        cpe_log(CPE_DEB, "buffer completely sent (count %d)", ctx->nc_count);
        rv = cpe_pollset_update(pfd, pfd->reqevents & ~APR_POLLOUT);
        assert(rv == APR_SUCCESS);
    }
    return APR_SUCCESS;
}

/* Receive chunks and save them in the buffer.
 */
static apr_status_t
client_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    cpe_network_ctx *ctx = (cpe_network_ctx *) context;
    cpe_io_buf      *recvbuf = &ctx->nc_recv;
    apr_status_t     rv;
    apr_size_t       howmany;

    cpe_event_add(e);
    ctx->nc_count++;
    assert((pfd->rtnevents & APR_POLLOUT) == 0);

    howmany = recvbuf->buf_capacity;
    rv = cpe_recv(pfd->desc.s, recvbuf, &howmany);

    if (recvbuf->total >= g_expected_total) {
        cpe_log(CPE_DEB, "%s",
            "received expected amount, exiting from main loop");
        cpe_main_loop_terminate();
    }
    return APR_SUCCESS;
}

apr_status_t
test_init(conf_t *conf)
{
    apr_status_t rv;

    g_expected_total = MYBUFSIZE;
    conf->co_debug = CPE_INFO;
    conf->co_listen_port = SERVER_PORT;
    conf->co_loop_duration = cpe_time_from_msec(1000);
    CHECK(apr_pool_create(&conf->co_pool, NULL));

    plan_tests(12);

    return APR_SUCCESS;
}

/** A real network I/O test, with huge buffer to send so that we have to
 *  perform the send in multiple passes.
 */
apr_status_t
test_run(conf_t *conf)
{
    cpe_network_ctx  s_ctx, c_ctx;
    unsigned int     i;
    int             *buf_as_int;
    apr_int16_t      s_flags, c_flags;
    apr_time_t       runtime;

    memset(&s_ctx, 0, sizeof s_ctx);
    s_ctx.nc_send.buf = apr_pcalloc(conf->co_pool, MYBUFSIZE);
    s_ctx.nc_send.buf_capacity = MYBUFSIZE;
    assert(s_ctx.nc_send.buf != NULL);

    memset(&c_ctx, 0, sizeof c_ctx);
    c_ctx.nc_recv.buf = apr_pcalloc(conf->co_pool, MYBUFSIZE);
    c_ctx.nc_recv.buf_capacity = MYBUFSIZE;
    assert(c_ctx.nc_recv.buf != NULL);

    buf_as_int = (int *) s_ctx.nc_send.buf;
    for (i = 0; i < s_ctx.nc_send.buf_capacity / sizeof(int); i++) {
        buf_as_int[i] = rand();
    }
    s_ctx.nc_send.buf_len = s_ctx.nc_send.buf_capacity;

    s_flags = APR_POLLOUT;
    c_flags = APR_POLLIN;
    test_network_io1(conf, &runtime,
        server_cb, &s_ctx, s_flags, NULL, NULL,
        client_cb, &c_ctx, c_flags, NULL, NULL);

    /* Note that we don't do the runtime > loop duration test because in
     * this case we expect to finish *earlier* than the loop duration.
     */

    ok(s_ctx.nc_send.total == c_ctx.nc_recv.total,
        "total sent = total received (s %d r %d)", s_ctx.nc_send.total,
        c_ctx.nc_recv.total);
    ok(s_ctx.nc_send.total == MYBUFSIZE, "total sent as expected (s %d e %d)",
        s_ctx.nc_send.total, MYBUFSIZE);
    ok(memcmp(s_ctx.nc_send.buf, c_ctx.nc_recv.buf, s_ctx.nc_send.buf_len) == 0,
        "send and receive buffers are identical");
    return APR_SUCCESS;
}
