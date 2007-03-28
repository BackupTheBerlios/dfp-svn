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

#define NELEMS 10
#define MYBUFSIZE 5 * ONE_SI_MEGA

typedef struct msg_hdr_ {
    uint16_t msg_version;
    uint16_t msg_type;
    uint32_t msg_length;
} msg_hdr_t;

static int          g_expected_total = 15 * MYBUFSIZE;


/* Schedule the sending of data on the connected socket (server side).
 */
static apr_status_t
timer_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    test_timer_ctx_t  *ctx = context;
    apr_status_t       rv;
    cpe_io_buf        *iobuf;

    cpe_log(CPE_DEB, "%s", "enter");
    pfd = NULL;

    /* NOTE difference between total_in_queue and total_sent
     */
    if (ctx->t_nc->nc_sendQ->cq_total_in_queue >= g_expected_total) {
        cpe_log(CPE_DEB, "sent as expected (s %d, e %d), disarming send timer",
            ctx->t_nc->nc_sendQ->cq_total_in_queue, g_expected_total);
        return APR_SUCCESS;
    }
    cpe_event_add(e);

    if (!find_next_avail_buffer(ctx)) {
        cpe_log(CPE_DEB, "%s", "all buffers in queue, skipping send");
        return APR_EGENERAL;
    }
    /* Put something in this buf and enqueue it.
     */
    iobuf = ctx->t_iobufs[ctx->t_index];
    iobuf->buf_len = iobuf->buf_capacity;

    cpe_log(CPE_DEB, "queuing buf %d", ctx->t_index);
    CHECK(cpe_send_enqueue(ctx->t_nc->nc_sendQ, iobuf));

    return APR_SUCCESS;
}

static apr_status_t
get_msg_size_cb(cpe_io_buf *iobuf, int *msg_size)
{
    msg_hdr_t *hdr;

    *msg_size = 0;
    hdr = (msg_hdr_t *) &iobuf->buf[0];
    *msg_size = ntohl(hdr->msg_length);
    return APR_SUCCESS;
}

/* NOTE it is the responsability of this function to deallocate the memory
 * for iobuf.
 */
static apr_status_t
msg_handler_cb(cpe_io_buf *iobuf, cpe_network_ctx *nctx)
{
    /* do work */

    cpe_log(CPE_DEB, "%s", "enter");
    cpe_iobuf_destroy(&iobuf, nctx);
    return APR_SUCCESS;
}

/* Event callback on the accepted socket.
 */
static apr_status_t
server_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    cpe_network_ctx *nctx = (cpe_network_ctx *) context;
    apr_status_t     rv;

    cpe_event_add(e);
    nctx->nc_count++;

    if (pfd->rtnevents & APR_POLLOUT) {
        rv = cpe_sender(nctx);
    }
    if (pfd->rtnevents & APR_POLLIN) {
        rv = cpe_receiver(nctx, pfd, 2000, g_conf.co_pool,
            sizeof(msg_hdr_t), get_msg_size_cb, msg_handler_cb);
    }
    return rv;
}

/* Event callback on the connected socket.
 */
static apr_status_t
client_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    cpe_network_ctx *nctx = (cpe_network_ctx *) context;
    cpe_io_buf      *recvbuf = &nctx->nc_recv;
    apr_status_t     rv;
    apr_size_t       howmany;

    cpe_event_add(e);
    nctx->nc_count++;

    howmany = recvbuf->buf_capacity;
    rv = cpe_recv(pfd->desc.s, recvbuf, &howmany);
    if (recvbuf->buf_offset == recvbuf->buf_capacity) {
        /* simply discard the data */
        recvbuf->buf_offset = 0;
        recvbuf->buf_len = 0;
    }

#if 0
    /* example usage of cpe_main_loop_terminate() */
    if (recvbuf->total >= g_expected_total) {
        cpe_log(CPE_DEB, "%s",
            "received expected amount, exiting from main loop");
        cpe_main_loop_terminate();
    }
#endif
    return APR_SUCCESS;
}

static apr_status_t
server_one_shot_cb(void *context, apr_pollfd_t *pfd, cpe_event *event)
{
    apr_status_t      rv;
    cpe_network_ctx  *nc = context;
    test_timer_ctx_t *timer_ctx;

    cpe_log(CPE_DEB, "%s", "enter");

    /* Setup timer event that will be used on the client side.
     */
    CHECK(timer_init(&timer_ctx, nc, nc->nc_pool, NELEMS, MYBUFSIZE));
    event = cpe_event_timer_create(cpe_time_from_msec(10), timer_cb, timer_ctx);
    ok(event != NULL, "create timer event");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event");

    CHECK(cpe_queue_init(&nc->nc_sendQ, pfd, nc->nc_pool, event));
    return rv;
}

static apr_status_t
client_one_shot_cb(void *context, apr_pollfd_t *pfd, cpe_event *event)
{
    apr_status_t     rv;
    cpe_network_ctx *nc = (cpe_network_ctx *) context;

    cpe_log(CPE_DEB, "%s", "enter");
    CHECK(cpe_queue_init(&nc->nc_sendQ, pfd, nc->nc_pool, event));
    return rv;
}

apr_status_t
test_init(conf_t *conf)
{
    apr_status_t rv;

    conf->co_debug = CPE_INFO;
    conf->co_listen_port = SERVER_PORT;
    conf->co_loop_duration = cpe_time_from_msec(500);
    CHECK(apr_pool_create(&conf->co_pool, NULL));

    plan_tests(13);

    return APR_SUCCESS;
}

/** A network I/O test, testing cpe_sender() and a mix of
 *  cpe_recv() / cpe_receiver().
 */
apr_status_t
test_run(conf_t *conf)
{
    cpe_network_ctx  s_ctx, c_ctx;
    apr_int16_t      s_flags, c_flags;
    apr_time_t       runtime;
    cpe_queue_t      *c_sendQ, *s_sendQ;
    apr_status_t     rv;

    /* Setup server context.
     */
    memset(&s_ctx, 0, sizeof s_ctx);
    s_ctx.nc_pool = conf->co_pool;
    s_flags = APR_POLLOUT | APR_POLLIN;

    /* Setup client context.
     */
    memset(&c_ctx, 0, sizeof c_ctx);
    c_ctx.nc_pool = conf->co_pool;
    c_flags = APR_POLLIN;
    CHECK(cpe_iobuf_init(&c_ctx.nc_recv, MYBUFSIZE, conf->co_pool));

    /* Run the test.
     */
    test_network_io1(conf, &runtime,
        server_cb, &s_ctx, s_flags, server_one_shot_cb, conf,
        client_cb, &c_ctx, c_flags, client_one_shot_cb, conf);

    /* Perform checks on the status at the end of the test.
     */
    s_sendQ = s_ctx.nc_sendQ;
    c_sendQ = c_ctx.nc_sendQ;

    ok(s_sendQ->cq_total_sent > 0, "sent something on server side (%d)",
        s_sendQ->cq_total_sent);
    ok(s_sendQ->cq_total_sent == c_ctx.nc_total_received,
        "total sent server side == total received client side (s %d r %d)",
        s_sendQ->cq_total_sent, c_ctx.nc_total_received);
    return APR_SUCCESS;
}
