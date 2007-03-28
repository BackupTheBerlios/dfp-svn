/*
 * Cisco Portable Events (CPE)
 *
 * $Id: test-cpe-8.c 136 2007-03-27 12:27:01Z mmolteni $
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
#define MYBUFSIZE (1 * ONE_SI_KILO)

/* Not yet used. */
enum mystate {
    Q,
};

#define PING 0xcafe
#define PONG 0xfade
typedef struct msg_hdr_ {
    uint16_t msg_version;
    uint16_t msg_type;
    uint32_t msg_length;
} msg_hdr_t;




/* Schedule the sending of data on the client side.
 */
static apr_status_t
timer_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    test_timer_ctx_t  *ctx = context;
    apr_status_t       rv;
    cpe_io_buf        *iobuf;
    msg_hdr_t         *hdr;

    pfd = NULL;
    cpe_event_add(e);

    if (!find_next_avail_buffer(ctx)) {
        cpe_log(CPE_DEB, "%s", "all buffers in queue, skipping send");
        return APR_EGENERAL;
    }
    /* Put something in this buf and enqueue it.
     */
    iobuf = ctx->t_iobufs[ctx->t_index];
    iobuf->buf_len = iobuf->buf_capacity;

    hdr = (msg_hdr_t *) &iobuf->buf[0];
    hdr->msg_type = htons(PING);
    hdr->msg_length = htonl(iobuf->buf_len);

    cpe_log(CPE_DEB, "queuing buf %d", ctx->t_index);
    CHECK(cpe_send_enqueue(ctx->t_nc->nc_sendQ, iobuf));

    return APR_SUCCESS;
}

static apr_status_t
get_msg_size_cb(cpe_io_buf *iobuf, int *msg_len)
{
    msg_hdr_t *hdr;

    *msg_len = 0;
    hdr = (msg_hdr_t *) &iobuf->buf[0];
    *msg_len = ntohl(hdr->msg_length);
    return APR_SUCCESS;
}

/* NOTE it is the responsability of this function to deallocate the memory
 * for iobuf.
 */
static apr_status_t
server_msg_handler_cb(cpe_io_buf *iobuf, cpe_network_ctx *nctx)
{
    msg_hdr_t    *hdr;
    apr_status_t  rv;

    hdr = (msg_hdr_t *) &iobuf->buf[0];

    /* we expect a PING */
    assert(ntohs(hdr->msg_type) == PING);

    /* transform it in a PONG and send it back */
    hdr->msg_type = htons(PONG);

    iobuf->buf_offset = 0; /* required for a successful enqueue */

    /* We cannot deallocate the buf right away, because we have to send it,
     * so we specify that it is cpe_sender's responsability to deallocate the
     * buffer once done.
     */
    iobuf->destroy = 1;
    cpe_log(CPE_DEB, "send queuing buf %p, marked to be destroyed", iobuf);
    CHECK(cpe_send_enqueue(nctx->nc_sendQ, iobuf));

    return APR_SUCCESS;
}

/* NOTE it is the responsability of this function to deallocate the memory
 * for iobuf.
 */
static apr_status_t
client_msg_handler_cb(cpe_io_buf *iobuf, cpe_network_ctx *nctx)
{
    /* in this case, we just discard the message
     */
    cpe_log(CPE_DEB, "consumed iobuf %p, discarding it", iobuf);
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
        rv = cpe_receiver(nctx, pfd, 2000, nctx->nc_pool,
            sizeof(msg_hdr_t), get_msg_size_cb, server_msg_handler_cb);
    }
    return rv;
}

/* Event callback on the connected socket.
 */
static apr_status_t
client_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    cpe_network_ctx *nctx = (cpe_network_ctx *) context;
    apr_status_t     rv;

    cpe_event_add(e);
    nctx->nc_count++;

    if (pfd->rtnevents & APR_POLLOUT) {
        rv = cpe_sender(nctx);
    }
    if (pfd->rtnevents & APR_POLLIN) {
        rv = cpe_receiver(nctx, pfd, 2000, nctx->nc_pool,
            sizeof(msg_hdr_t), get_msg_size_cb, client_msg_handler_cb);
    }
    return rv;
}

static apr_status_t
server_one_shot_cb(void *context, apr_pollfd_t *pfd, cpe_event *event)
{
    apr_status_t     rv;
    cpe_network_ctx *ctx = (cpe_network_ctx *) context;

    cpe_log(CPE_DEB, "%s", "enter");
    CHECK(cpe_queue_init(&ctx->nc_sendQ, pfd, ctx->nc_pool, event));
    return rv;
}

static apr_status_t
client_one_shot_cb(void *context, apr_pollfd_t *pfd, cpe_event *event)
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

apr_status_t
test_init(conf_t *conf)
{
    apr_status_t rv;

    conf->co_debug = CPE_INFO;
    conf->co_listen_port = SERVER_PORT;
    conf->co_loop_duration = cpe_time_from_msec(500);
    CHECK(apr_pool_create(&conf->co_pool, NULL));

    plan_tests(15);

    return APR_SUCCESS;
}

/** A network I/O test, testing only cpe_sender() and cpe_receiver().
 *  This should also show the proper way of using CPE from a client program.
 *
 *             CLIENT SIDE  |  SERVER SIDE
 *                          |
 *  send PING      ================>   receive
 *  in a timed loop                       |
 *                          |             v
 *                          |    do something on data (from PING to PONG)
 *                          |             |
 *                          |             v
 *  receive    <======================  send
 *                          |
 */
apr_status_t
test_run(conf_t *conf)
{
    cpe_network_ctx  s_ctx, c_ctx;
    apr_int16_t      s_flags, c_flags;
    apr_time_t       runtime;
    cpe_queue_t      *c_sendQ, *s_sendQ;

    /* Setup server context.
     */
    memset(&s_ctx, 0, sizeof s_ctx);
    s_ctx.nc_pool = conf->co_pool;
    s_flags = APR_POLLOUT | APR_POLLIN;

    /* Setup client context.
     */
    memset(&c_ctx, 0, sizeof c_ctx);
    c_ctx.nc_pool = conf->co_pool;
    c_flags = APR_POLLOUT | APR_POLLIN;

    /* Run the test.
     */
    test_network_io1(conf, &runtime,
        server_cb, &s_ctx, s_flags, server_one_shot_cb, &s_ctx,
        client_cb, &c_ctx, c_flags, client_one_shot_cb, &c_ctx);

    /* Perform checks on the status at the end of the test.
     */
    s_sendQ = s_ctx.nc_sendQ;
    c_sendQ = c_ctx.nc_sendQ;

    ok(c_sendQ->cq_total_sent > 0, "sent something on client side (%d)",
        c_sendQ->cq_total_sent);
    ok(c_sendQ->cq_total_sent == s_ctx.nc_total_received,
        "total sent client side == total received server side (s %d r %d)",
        c_sendQ->cq_total_sent, s_ctx.nc_total_received);
    ok(s_sendQ->cq_total_sent > 0, "sent something on server side (%d)",
        s_sendQ->cq_total_sent);
    ok(s_sendQ->cq_total_sent == c_ctx.nc_total_received,
        "total sent server side == total received client side (s %d r %d)",
        s_sendQ->cq_total_sent, c_ctx.nc_total_received);

    return APR_SUCCESS;
}
