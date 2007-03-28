/*
 * Cisco Portable Events (CPE)
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

#define MYBUFSIZE 10 * ONE_SI_MEGA

/* send */
static apr_status_t
server_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    cpe_network_ctx *ctx = (cpe_network_ctx *) context;
    cpe_io_buf      *sendbuf = &ctx->nc_send;
    apr_status_t     rv;
    apr_size_t       len;

    cpe_event_add(e);
    ctx->nc_count++;
    assert((pfd->rtnevents & APR_POLLIN) == 0);

    len = sendbuf->buf_len - sendbuf->buf_offset;
    rv = apr_socket_send(pfd->desc.s, &sendbuf->buf[sendbuf->buf_offset], &len);
    if (rv != APR_SUCCESS) {
        cpe_log(CPE_DEB, "apr_socket_send: %s", cpe_errmsg(rv));
    }
    sendbuf->buf_offset += len;
    sendbuf->total += len;
    cpe_log(CPE_DEB, "sent %d bytes (offset %d, count %d)", len,
        sendbuf->buf_offset, ctx->nc_count);
    if (sendbuf->buf_offset >= sendbuf->buf_len) {
        cpe_log(CPE_DEB, "buffer completely sent (count %d)",
            ctx->nc_count);
        sendbuf->buf_offset = 0;
        rv = cpe_pollset_update(pfd, pfd->reqevents & ~APR_POLLOUT);
        assert(rv == APR_SUCCESS);
    }

    return APR_SUCCESS;
}

/* receive */
static apr_status_t
client_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    cpe_network_ctx *ctx = (cpe_network_ctx *) context;
    cpe_io_buf      *recvbuf = &ctx->nc_recv;
    apr_status_t     rv;
    apr_size_t       len;

    cpe_event_add(e);
    ctx->nc_count++;
    assert((pfd->rtnevents & APR_POLLOUT) == 0);
    if (recvbuf->buf_offset >= recvbuf->buf_capacity) {
        cpe_log(CPE_DEB, "buffer full (count %d)", ctx->nc_count);
        return APR_EGENERAL;
    }

    len = recvbuf->buf_capacity - recvbuf->buf_offset;
    rv = apr_socket_recv(pfd->desc.s, &recvbuf->buf[recvbuf->buf_offset], &len);
    if (rv != APR_SUCCESS) {
        cpe_log(CPE_DEB, "apr_socket_recv: %s", cpe_errmsg(rv));
    }
    recvbuf->buf_offset += len;
    recvbuf->total += len;
    recvbuf->buf_len += len;
    cpe_log(CPE_DEB, "received %d bytes (offset %d, count %d)", len,
        recvbuf->buf_offset, ctx->nc_count);

    return APR_SUCCESS;
}

apr_status_t
test_init(conf_t *conf)
{
    apr_status_t rv;

    conf->co_debug = CPE_INFO;
    conf->co_listen_port = SERVER_PORT;
    conf->co_loop_duration = cpe_time_from_msec(300);
    CHECK(apr_pool_create(&conf->co_pool, NULL));

    plan_tests(13);

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

    // As is, this is a bit too coarse grained.
    ok(runtime > conf->co_loop_duration,
        "runtime > loop duration (delta %lld ms)",
        apr_time_as_msec(runtime - conf->co_loop_duration));

    ok(s_ctx.nc_send.total == c_ctx.nc_recv.total,
        "total sent = total received (s %d r %d)", s_ctx.nc_send.total,
        c_ctx.nc_recv.total);
    ok(s_ctx.nc_send.total == MYBUFSIZE, "total sent as expected (s %d e %d)",
        s_ctx.nc_send.total, MYBUFSIZE);
    ok(memcmp(s_ctx.nc_send.buf, c_ctx.nc_recv.buf, s_ctx.nc_send.buf_len) == 0,
        "send and receive buffers are identical");
    return APR_SUCCESS;
}
