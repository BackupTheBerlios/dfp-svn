/*
 * Cisco Portable Events (CPE)
 * $Id: cpe-network.c 136 2007-03-27 12:27:01Z mmolteni $
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


/* Credits:
 * some code based on the examples in the APR tutorial at
 * http://dev.ariel-networks.com/apr/
 * Copyright 2005 INOUE Seiichiro <inoue&ariel-networks.com>
 */

#include <assert.h>

#include "cpe.h"
#include "cpe-network.h"
#include "cpe-logging.h"
#include "cpe-private.h"


struct cpe_socket_prepare_ctx_ {
    cpe_afilter_t    pc_afilter;
    cpe_callback_t   pc_callback;
    void            *pc_ctx1;
    apr_int16_t      pc_reqevents;
    apr_socket_t    *pc_orig_socket;
    cpe_event       *pc_event;
    int              pc_max_peers;
    int              pc_num_peers;
    cpe_callback_t   pc_one_shot_cb;
    void            *pc_one_shot_ctx;
};

static apr_sockaddr_t *g_cpe_sockaddr_localhost;


/*! Create a non-blocking client socket, ready to be passed to
 *  cpe_socket_after_connect().
 *
 *  Parameters are as for apr_sockaddr_info_get() and apr_socket_create().
 */
apr_status_t
cpe_socket_client_create(apr_socket_t **sock, apr_sockaddr_t **sockaddr,
    const char *hostname, apr_port_t port, apr_pool_t *pool)
{
    apr_status_t rv;

    *sock = NULL;
    *sockaddr = NULL;
    rv = apr_sockaddr_info_get(sockaddr, hostname, APR_INET, port, 0, pool);
    if (rv != APR_SUCCESS) {
	return rv;
    }
    rv = apr_socket_create(sock, (*sockaddr)->family, SOCK_STREAM,
        APR_PROTO_TCP, pool);
    if (rv != APR_SUCCESS) {
	return rv;
    }
    cpe_log(CPE_DEB, "created socket %p", *sock);
    /* Critical for CPE: set the socket non-blocking. */
    apr_socket_opt_set(*sock, APR_SO_NONBLOCK, 1);
    apr_socket_timeout_set(*sock, 0);

    return APR_SUCCESS;
}


/*! Create a non-blocking server socket, ready to be passed to
 *  cpe_socket_after_accept().
 *
 *  Parameters are as for apr_sockaddr_info_get(), apr_socket_create(),
 *  apr_socket_bind() and apr_socket_listen().
 *
 */
apr_status_t
cpe_socket_server_create(apr_socket_t **sock, apr_sockaddr_t **sockaddr,
    const char *hostname, apr_port_t port, apr_int32_t backlog,
    apr_pool_t *pool)
{
    apr_status_t    rv;

    CHECK(cpe_socket_client_create(sock, sockaddr, hostname, port, pool));
    CHECK(apr_socket_opt_set(*sock, APR_SO_REUSEADDR, 1));
    CHECK(apr_socket_bind(*sock, *sockaddr));
    CHECK(apr_socket_listen(*sock, backlog));
    return APR_SUCCESS;
}


static apr_status_t
cpe_filter_localhost(apr_sockaddr_t *incoming)
{
    if (apr_sockaddr_equal(incoming, g_cpe_sockaddr_localhost)) {
        return APR_SUCCESS;
    }
    return APR_EGENERAL;
}


apr_status_t
cpe_filter_any(apr_sockaddr_t *incoming)
{
    incoming = NULL;
    return APR_SUCCESS;
}


static apr_status_t
cpe_increment_peers(cpe_socket_prepare_ctx *ctx)
{
    if (ctx->pc_num_peers >= ctx->pc_max_peers) {
        cpe_log(CPE_DEB, "cannot increment, num_peers %d, max_peers %d",
            ctx->pc_num_peers, ctx->pc_max_peers);
        return APR_EGENERAL;
    }
    ctx->pc_num_peers++;
    cpe_log(CPE_DEB, "num_peers incremented to %d", ctx->pc_num_peers);
    return APR_SUCCESS;
}


/** Called by cpe_socket_cleanup_cb.
 */
static apr_status_t
cpe_decrement_peers(cpe_socket_prepare_ctx *ctx)
{
    if (ctx->pc_num_peers <= 0) {
        cpe_log(CPE_ERR, "cannot decrement num_peers (%d)",
            ctx->pc_num_peers);
        return APR_EGENERAL;
    }
    ctx->pc_num_peers--;
    cpe_log(CPE_DEB, "num_peers decremented to %d", ctx->pc_num_peers);
    return APR_SUCCESS;
}


/** Cleanup callback called in theory apr_socket_close().
 */
static apr_status_t
cpe_socket_cleanup_cb(void *context)
{
    apr_status_t            rv;
    cpe_socket_prepare_ctx *ctx;

    cpe_log(CPE_DEB, "%s", "enter");
    ctx = context;
    if (ctx == NULL) {
        /* this is not an error condition */
        cpe_log(CPE_DEB, "%s", "invoked with NULL args, returning");
        return APR_SUCCESS;
    }
    CHECK(cpe_decrement_peers(ctx));
    return rv;
}


/** Wrapper around apr_socket_close() to invoke the cleanup callback.
 */
apr_status_t
cpe_socket_close(apr_socket_t *sock)
{
    apr_status_t            rv;
    void                   *p = NULL;

    apr_socket_close(sock);
    CHECK(apr_socket_data_get(&p, "dummykey", sock));
    CHECK(cpe_socket_cleanup_cb(p));
    return rv;
}

/*! Internal use callback associated with cpe_socket_after_accept().
 *  Handle the accept and put the new accepted socket in the event system.
 */
static apr_status_t
cpe_socket_accept_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    apr_socket_t           *newsock;
    apr_status_t            rv;
    apr_sockaddr_t         *sockaddr;
    cpe_socket_prepare_ctx *ctx;
    cpe_event              *event;
    char                   *hostip;

    ctx = context;
    assert(ctx != NULL);
    pfd = NULL;
    cpe_event_add(e);

    CHECK(rv = apr_socket_accept(&newsock, ctx->pc_orig_socket, g_cpe_pool));
    rv = apr_socket_addr_get(&sockaddr, APR_REMOTE, newsock);
    if (rv == APR_SUCCESS) {
        apr_sockaddr_ip_get(&hostip, sockaddr);
    } else {
        cpe_log(CPE_ERR, "apr_sock_addr_get: %s", cpe_errmsg(rv));
        cpe_socket_close(newsock);
        return rv;
    }
    if (cpe_increment_peers(ctx) != APR_SUCCESS) {
        cpe_socket_close(newsock);
        cpe_log(CPE_WARN, "rejected connection from %s %d (too many)",
            hostip, sockaddr->port);
        return APR_EGENERAL;
    }
    /* Useless because APR doesn't call the callback on socket close (as
     * documented), but on pool destruction. So for the time being we use
     * our wrapper cpe_socket_close()
     */
    CHECK(apr_socket_data_set(newsock, ctx, "dummykey", cpe_socket_cleanup_cb));

    /* SECURITY: if the user didn't specify an accept filter, accept
     * only connections from localhost
     */
    if (ctx->pc_afilter != NULL) {
        rv = ctx->pc_afilter(sockaddr);
    } else {
        rv = cpe_filter_localhost(sockaddr);
    }
    if (rv != APR_SUCCESS) {
        cpe_socket_close(newsock);
        cpe_log(CPE_WARN, "rejected connection from %s %d (filtered)",
            hostip, sockaddr->port);
        return rv;
    }
    cpe_log(CPE_INFO, "accepted connection from %s %d", hostip,
        sockaddr->port);

    /* Critical for CPE: set the socket non-blocking. */
    apr_socket_opt_set(newsock, APR_SO_NONBLOCK, 1);
    apr_socket_timeout_set(newsock, 0);

    /*
     * Create an event for the new socket and add it to the event system.
     */
    cpe_log(CPE_DEB, "new accepted socket %p, creating event", newsock);
    event = cpe_event_fdesc_create(APR_POLL_SOCKET, ctx->pc_reqevents,
        (apr_descriptor) newsock, 0, ctx->pc_callback, ctx->pc_ctx1);
    if (event == NULL) {
        cpe_log(CPE_ERR, "%s", "cpe_event_fdesc_create fail");
        return APR_EGENERAL;
    }
    CHECK(cpe_event_add(event));
    if (ctx->pc_one_shot_cb != NULL) {
        CHECK(ctx->pc_one_shot_cb(ctx->pc_one_shot_ctx, &event->ev_pollfd, event));
    }
    return rv;
}


/*! Setup the required steps to accept a connection (fully async) and
 *  install an event/callback to be called on the newly created socket.
 *
 * @param lsock        Listening socket, created by cpe_socket_server_create().
 * @param callback     Callback that will be called on the new socket.
 * @param ctx1         Callback argument.
 * @param pfd_flags    Poll flags for the new socket.
 * @param afilter_cb   Optional accept filter callback. NULL will accept
 *                     connections only from localhost.
 * @param max_peers    Max number of contemporary connections.
 * @param one_shot_cb  Callback that will be called once per newly created
 *                     socket. This allows e.g. to call cpe_queue_init with the
 *                     appropriate pfd.
 * @param one_shot_ctx Callback argument.
 * @param pool         Memory pool to use.
 */
apr_status_t
cpe_socket_after_accept(
    apr_socket_t   *lsock,
    cpe_callback_t  callback,
    void           *ctx1,
    apr_int16_t     pfd_flags,
    cpe_afilter_t   afilter_cb,
    int             max_peers,
    cpe_callback_t  one_shot_cb,
    void           *one_shot_ctx,
    apr_pool_t     *pool)
{
    apr_status_t            rv;
    cpe_event              *event;
    cpe_socket_prepare_ctx *sp_ctx;
    int                     nonblock;

    /* Enforce contract. */
    apr_socket_opt_get(lsock, APR_SO_NONBLOCK, &nonblock);
    assert(nonblock == 1);

    if (max_peers <= 0 || max_peers > CPE_MAX_PEERS) {
        cpe_log(CPE_DEB, "%s", "max_peers outside range (%d)");
        return APR_EINVAL;
    }
    /*
     * Setup event/callback for listening socket.
     */
    cpe_log(CPE_DEB, "%s", "creating event for listening socket");
    sp_ctx = apr_palloc(pool, sizeof *sp_ctx);
    if (sp_ctx == NULL) {
        return APR_EGENERAL;
    }
    event = cpe_event_fdesc_create(APR_POLL_SOCKET, APR_POLLIN,
        (apr_descriptor) lsock, 0, cpe_socket_accept_cb, sp_ctx);
    if (event == NULL) {
        return APR_EGENERAL;
    }
    sp_ctx->pc_afilter      = afilter_cb;
    sp_ctx->pc_callback     = callback;
    sp_ctx->pc_ctx1         = ctx1;
    sp_ctx->pc_reqevents    = pfd_flags;
    sp_ctx->pc_orig_socket  = lsock;
    sp_ctx->pc_event        = event;
    sp_ctx->pc_max_peers    = max_peers;
    sp_ctx->pc_num_peers    = 0;
    sp_ctx->pc_one_shot_cb  = one_shot_cb;
    sp_ctx->pc_one_shot_ctx = one_shot_ctx;

    rv = cpe_event_add(event);
    if (rv != APR_SUCCESS) {
        return rv;
    }
    return APR_SUCCESS;
}


/*! Internal use callback associated with cpe_socket_after_connect().
 *
 * This function is not really needed. We keep it for simmetry with the
 * accept case, but we might remove it once we make up our mind.
 */
static apr_status_t
cpe_socket_connect_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    cpe_socket_prepare_ctx *ctx;
    cpe_event              *event;
    apr_status_t            rv;

    //! @todo WRITEME. How to check for timeout?
    //if (timeout) {
    //    cpe_socket_close(sock);
    //}
    ctx = (cpe_socket_prepare_ctx *) context;
    assert(ctx != NULL);
    // @todo FIXME UGLY e and event are the same!!!
    cpe_event_add(e);
    event = ctx->pc_event;
    assert(event != NULL);

    cpe_log(CPE_DEB, "%s", "connection established");
    cpe_log(CPE_DEB, "ctx orig socket %p, pfd socket %p",
        ctx->pc_orig_socket, pfd->desc.s);

    /*
     * Fixup the existing event.
     */
    event->ev_callback         = ctx->pc_callback;
    event->ev_ctx              = ctx->pc_ctx1;
    rv = cpe_pollset_update(&event->ev_pollfd, ctx->pc_reqevents);
    if (rv != APR_SUCCESS) {
        cpe_log(CPE_ERR, "cpe_pollset_update: %s", cpe_errmsg(rv));
    }
    if (ctx->pc_one_shot_cb != NULL) {
        CHECK(ctx->pc_one_shot_cb(ctx->pc_one_shot_ctx, &event->ev_pollfd, event));
    }
    return APR_SUCCESS;
}


/*! Start a connect (async) and once done install an event/callback
 * associated to the connected socket.
 *
 * @param csock        connecting socket, created by cpe_socket_client_create()
 * @param sockaddr     address infos, created by cpe_socket_client_create()
 * @param timeous_us   optional timeout in us to connect. 0 Means no timeout.
 * @param callback     callback that will be called on the connected socket
 * @param context      callback argument
 * @param pfd_flags    poll flags for the connected socket
 * @param pool         memory pool to use
 */
apr_status_t
cpe_socket_after_connect(
    apr_socket_t   *csock,
    apr_sockaddr_t *sockaddr,
    apr_time_t      timeout_us,
    cpe_callback_t  callback,
    void           *context,
    apr_int16_t     pfd_flags,
    cpe_callback_t  one_shot_cb,
    void           *one_shot_ctx,
    apr_pool_t     *pool)
{
    apr_status_t            rv;
    cpe_event              *event;
    cpe_socket_prepare_ctx *ctx;
    int                     nonblock;

    /* Enforce contract. */
    apr_socket_opt_get(csock, APR_SO_NONBLOCK, &nonblock);
    assert(nonblock == 1);

    /*
     * Setup event/callback for connecting socket.
     */
    cpe_log(CPE_DEB, "%s", "creating event for connecting socket");
    ctx = apr_palloc(pool, sizeof *ctx);
    if (ctx == NULL) {
        return APR_EGENERAL;
    }
    event = cpe_event_fdesc_create(APR_POLL_SOCKET, APR_POLLOUT,
        (apr_descriptor) csock, timeout_us, cpe_socket_connect_cb, ctx);
    if (event == NULL) {
        return APR_EGENERAL;
    }
    ctx->pc_callback     = callback;
    ctx->pc_ctx1         = context;
    ctx->pc_reqevents    = pfd_flags;
    ctx->pc_orig_socket  = csock;
    ctx->pc_event        = event;
    ctx->pc_one_shot_cb  = one_shot_cb;
    ctx->pc_one_shot_ctx = one_shot_ctx;

    CHECK(cpe_event_add(event));

    rv = apr_socket_connect(csock, sockaddr);
    if (rv != APR_SUCCESS && ! APR_STATUS_IS_EINPROGRESS(rv)) {
        cpe_log(CPE_DEB, "apr_socket_connect %s", cpe_errmsg(rv));
        return rv;
    }
    return APR_SUCCESS;
}


/** Send at max \p howmany bytes on socket \p sock from buffer \p sendbuf.
 *  Parameter \p howmany allows to send less than sendbuf->buf_len.
 *  Setting \p howmany to sendbuf->buf_len means send as much as you can (offset
 *  is taken into consideration).
 */
apr_status_t
cpe_send(
    apr_socket_t *sock,    /** Socket to send to. */
    cpe_io_buf   *sendbuf, /** Buffer to send data from. */
    apr_size_t   *howmany) /** On entry, the max number of bytes to send; on
                               exit, the number of bytes sent. */
{
    apr_status_t  rv;
    apr_size_t    len;

    assert(sendbuf->buf_len <= sendbuf->buf_capacity);
    if (*howmany <= 0) {
        return APR_EINVAL;
    }
    assert(sendbuf->buf_len >= sendbuf->buf_offset);
    len = sendbuf->buf_len - sendbuf->buf_offset;
    if (len == 0) {
        cpe_log(CPE_DEB, "iobuf %p no buffer space (socket %p)", sendbuf, sock);
        return APR_EGENERAL;
    }
    len = cpe_min(len, *howmany);
    rv = apr_socket_send(sock, &sendbuf->buf[sendbuf->buf_offset], &len);
    if (rv != APR_SUCCESS) {
        cpe_log(CPE_DEB, "apr_socket_send: %s", cpe_errmsg(rv));
        /* Not clear what to do here; should look at the returned value of
         * len? Should consider APR_EOF ?
         */
    }
    *howmany = len;
    sendbuf->buf_offset += len;
    sendbuf->total += len;
    cpe_log(CPE_DEB, "sent %d bytes (offset %d, buf %p, socket %p)", len,
        sendbuf->buf_offset, sendbuf, sock);
    return rv;
}


/** Receive at max howmany bytes on socket sock and store them in recvbuf.
 *  Parameter howmany allows to receive less than the available space in
 *  recvbuf.
 *  Setting howmany to the same value as recvdbuf->buf_len means receive as
 *  much as you can (offset is taken into consideration).
 */
apr_status_t
cpe_recv(apr_socket_t *sock, cpe_io_buf *recvbuf, apr_size_t *howmany)
{
    apr_status_t  rv;
    apr_size_t    len;

    assert(recvbuf->buf_offset <= recvbuf->buf_capacity);
    if (*howmany <= 0) {
        cpe_log(CPE_DEB, "invalid howmany %d", *howmany);
        return APR_EINVAL;
    }
    len = recvbuf->buf_capacity - recvbuf->buf_offset;
    if (len == 0) {
        cpe_log(CPE_DEB, "no buffer space to receive on buffer %p (socket %p)",
            recvbuf, sock);
        return APR_EGENERAL;
    }
    len = cpe_min(len, *howmany);
    rv = apr_socket_recv(sock, &recvbuf->buf[recvbuf->buf_offset], &len);
    if (rv != APR_SUCCESS) {
        if (APR_STATUS_IS_EAGAIN(rv)) {
            cpe_log(CPE_ERR, "CPE client programming error: socket %p marked "
                "for non-blocking I/O and no data ready to be read.", sock);
        }
        cpe_log(CPE_DEB, "apr_socket_recv: %s (socket %p)", cpe_errmsg(rv),
            sock);
        /* Not clear what to do here; should look at the returned value of
         * len? Should consider APR_EOF ?
         */
    }
    *howmany = len;
    recvbuf->buf_offset += len;
    recvbuf->buf_len = recvbuf->buf_offset;
    recvbuf->total += len;
    cpe_log(CPE_DEB, "received %d bytes (offset %d, socket %p)", len,
        recvbuf->buf_offset, sock);
    return rv;
}


/** Put buffer sendbuf in send queue head, to be processed by cpe_sender().
 *  Perform synchronization on the pollfd associated with the queue (enables
 *  POLLOUT if needed).
 *
 *  @remark Memory management (allocation and deallocation) must be done by
 *  the caller; note also that this queue doesn't copy the buffer, so the
 *  buffer is safe to be deallocated/manipulated only if buf->inqueue == 0.
 */
apr_status_t
cpe_send_enqueue(cpe_queue_t *head, cpe_io_buf *sendbuf)
{
    apr_status_t rv;

    //assert(sendbuf->buf_offset == 0);
    if (sendbuf->buf_offset != 0) {
        cpe_log(CPE_DEB, "buffer %p has non-zero offset (%d), cannot enqueue",
            sendbuf, sendbuf->buf_offset);
        return APR_EINVAL;
    }
    if (sendbuf->buf_len == 0) {
        cpe_log(CPE_DEB, "buffer %p is empty, cannot enqueue", sendbuf);
        return APR_EINVAL;
    }
    if (sendbuf->inqueue) {
        cpe_log(CPE_DEB, "buffer %p already in queue %p (nelems %d)", sendbuf,
            head, head->cq_nelems);
        return APR_EINVAL;
    }

    /* FIFO queue, insert at the end */
    head->cq_nelems++;
    sendbuf->inqueue = 1;
    sendbuf->next = NULL;
    if (head->cq_next == NULL) {
        head->cq_next = sendbuf;
    } else {
        head->cq_tail->next = sendbuf;
    }
    head->cq_tail = sendbuf;
    head->cq_total_in_queue += sendbuf->buf_len;
    cpe_log(CPE_DEB, "inserted buf %p in queue %p (nelems %d)",
        sendbuf, head, head->cq_nelems);

    rv = cpe_pollset_update(head->cq_pfd, head->cq_pfd->reqevents | APR_POLLOUT);
    return rv;
}


/** Process send queue head, associated with socket/event pfd.
 *  Perform synchronization on the pollfd associated with the queue (disables
 *  POLLOUT if queue is empty).
 *
 *  @remark Memory management (allocation and deallocation) must be done by
 *  the caller of cpe_send_enqueue().
 */
apr_status_t
cpe_sender(cpe_network_ctx *nctx)
{
    cpe_io_buf   *sendbuf;
    apr_size_t    howmany;
    apr_status_t  rv;
    cpe_queue_t   *head;

    cpe_log(CPE_DEB, "%s", "enter");
    head = nctx->nc_sendQ;
    if (head->cq_next == NULL) {
        cpe_log(CPE_DEB, "queue %p (socket %p) is empty, disabling POLLOUT",
            head, head->cq_pfd->desc.s);
        rv = cpe_pollset_update(head->cq_pfd,
            head->cq_pfd->reqevents & ~APR_POLLOUT);
        assert(rv == APR_SUCCESS);
        return rv;
    }

    sendbuf = head->cq_next;
    howmany = sendbuf->buf_len;
    assert(howmany > 0);

    rv = cpe_send(head->cq_pfd->desc.s, sendbuf, &howmany);
    /* XXX what to do with rv ? */
    head->cq_total_sent += howmany;
    if (sendbuf->buf_offset == sendbuf->buf_len) {
        head->cq_nelems--;
        sendbuf->inqueue = 0;
        sendbuf->buf_offset = 0;
        cpe_log(CPE_DEB,
            "buffer %p (on queue %p, nelems %d, socket %p) completely sent",
            sendbuf, head, head->cq_nelems, head->cq_pfd->desc.s);
        /* Update queue to next buffer to send; remove buffer from queue.
         */
        if (sendbuf->next != NULL) {
            head->cq_next = sendbuf->next;
            sendbuf->next = NULL;
        } else {
            head->cq_next = NULL;
        }
        if (sendbuf->destroy) {
            cpe_log(CPE_DEB, "%s", "iobuf marked to be destroyed");
            cpe_iobuf_destroy(&sendbuf, nctx);
        }
    }
    return APR_SUCCESS;
}


/** Process incoming data, passing a complete message to a specified
 *  consumer.
 *
 * @param fixed_len       Specify the minimum header size containg enough
 *                        information for \p get_msg_size_cb.
 * @param get_msg_size_cb Determine the size of the full msg by looking at the
 *                        header.
 * @param msg_handler_cb  Message consumer.
 *
 * To be generic, perform a two-pass read (this notion is partly inspired by
 * Wireshark's tcp_dissect_pdus parameters):
 * - first pass: read the minimum necessary (\p fixed_len) to figure out the
 *   full packet size
 * - second pass: read the full packet, and pass it to the specified consumer
 *   (\p msg_handler_cb)
 *
 * @remark There is no queue; once a packet is read, it is passed to its
 * consumer. Memory for the iobuf is allocated here, from a new pool, and
 * must be deallocated by the consumer (another more advanced option, useful
 * in case the consumer wants to recycle the iobuf, is having the consumer
 * set iobuf->destroy, plus a cpe_send_enqueue(). Then cpe_sender() will
 * deallocate the buffer on behalf of the consumer.
 *
 * @remark There is a general resync problem with protocols on top of a
 * stream-oriented transport like TCP: if I don't find what I expected, how
 * do I known where is the start of the next PDU ? For example if the version
 * is not what I expected, then I have no guarantee at all that the header
 * layout is the same, as so how can I calculate the length ? So what we do
 * is to drop the connection.
 */
apr_status_t
cpe_receiver(cpe_network_ctx *nctx, apr_pollfd_t *pfd, int iobufsize,
    apr_pool_t *pool, int fixed_len, cpe_get_msg_size_t get_msg_size_cb,
    cpe_handle_msg_t msg_handler_cb)
{
    cpe_io_buf  *iobuf;
    apr_size_t   howmany;
    apr_status_t rv;

    cpe_log(CPE_DEB, "%s", "enter");
    if (get_msg_size_cb == NULL || msg_handler_cb == NULL) {
        cpe_log(CPE_ERR, "%s", "NULL callbacks");
        return APR_EINVAL;
    }

    iobuf = nctx->nc_iobuf;
    if (iobuf == NULL) {
        CHECK(cpe_iobuf_create(&iobuf, iobufsize, pool));
        nctx->nc_iobuf = iobuf;
    }

    /* first-pass read
     */
    if (iobuf->buf_len < fixed_len) {
        howmany = fixed_len - iobuf->buf_len;
        rv = cpe_recv(pfd->desc.s, iobuf, &howmany);
        nctx->nc_total_received += howmany;
        if (rv != APR_SUCCESS) {
            if (APR_STATUS_IS_EOF(rv)) {
                cpe_log(CPE_ERR, "%s", "remote end closed connection");
            } else {
                cpe_log(CPE_ERR, "%s", "cpe_recv() failed in first-pass");
            }
            cpe_iobuf_destroy(&iobuf, nctx);
            cpe_event_remove(pfd->client_data);
            pfd->client_data = NULL;
            cpe_socket_close(pfd->desc.s);
            cpe_resource_destroy_users(pfd->desc.s);
            pfd->desc.s = NULL;
            return rv;
        }
        if (iobuf->buf_len < fixed_len) {
            /* we haven't read enough; retry next time */
            cpe_log(CPE_DEB, "iobuf %p first-pass: buf_len %d, needed %d",
                iobuf, iobuf->buf_len, fixed_len);
            return APR_SUCCESS;
        }
        rv = get_msg_size_cb(iobuf, &nctx->nc_msg_size);
        if (rv != APR_SUCCESS) {
            cpe_log(CPE_ERR, "%s",
                "error in obtaining msg size, dropping connection");

            cpe_iobuf_destroy(&iobuf, nctx);
            cpe_event_remove(pfd->client_data);
            pfd->client_data = NULL;
            cpe_socket_close(pfd->desc.s);
            cpe_resource_destroy_users(pfd->desc.s);
            pfd->desc.s = NULL;
            return rv;
        }
        /* we are ready for the second-pass */
    }

    /* second-pass read
     */
    howmany = nctx->nc_msg_size - iobuf->buf_len;
    if (howmany > 0) {
        rv = cpe_recv(pfd->desc.s, iobuf, &howmany);
        nctx->nc_total_received += howmany;
        /* XXX an error could be a socket close on the other side; should
         * handle better
         */
        if (rv != APR_SUCCESS) {
            if (iobuf->buf_len < nctx->nc_msg_size) {
            cpe_log(CPE_ERR, "iobuf %p cpe_recv() failed in second-pass and "
                "not enough data to call consumer", iobuf);

                cpe_iobuf_destroy(&iobuf, nctx);
                cpe_event_remove(pfd->client_data);
                pfd->client_data = NULL;
                cpe_socket_close(pfd->desc.s);
                cpe_resource_destroy_users(pfd->desc.s);
                pfd->desc.s = NULL;
                return rv;
            } else {
                cpe_log(CPE_ERR, "iobuf %p cpe_recv() failed in second-pass but"
                    "enough data to call consumer", iobuf);
            }
        }
        if (iobuf->buf_len < nctx->nc_msg_size) {
            /* we haven't read enough; retry next time */
            cpe_log(CPE_DEB, "iobuf %p second-pass: buf_len %d, needed %d",
                iobuf, iobuf->buf_len, nctx->nc_msg_size);
            return APR_SUCCESS;
        }
    }
    cpe_log(CPE_DEB, "invoking callback %p on iobuf %p", msg_handler_cb, iobuf);
    return msg_handler_cb(iobuf, nctx);
}


apr_socket_t *
cpe_queue_get_socket(cpe_queue_t *head)
{
    if (head == NULL) {
        return NULL;
    }
    return head->cq_pfd->desc.s;
}

/** Create a CPE queue (resource associated to a socket).
 * @param event Resource user that will be destroyed when the resource is
 *              destroyed.
 * @param pfd   The socket associated is the resource we keep track of.
 */
apr_status_t
cpe_queue_init(cpe_queue_t **head, apr_pollfd_t *pfd, apr_pool_t *pool,
    cpe_event *event)
{
    apr_status_t rv;

    cpe_log(CPE_DEB, "%s", "enter");
    CHECK_NULL(*head, apr_pcalloc(pool, sizeof(cpe_queue_t)));
    (*head)->cq_pfd = pfd;
    /* If check fails we leak */
    CHECK(cpe_resource_register_user(pfd->desc.s, event));
    cpe_log(CPE_DEB, "initialized queue %p", *head);
    return APR_SUCCESS;
}


/** Create an iobuf from a new memory pool.
 */
apr_status_t
cpe_iobuf_create(cpe_io_buf **iobuf, int bufsize, apr_pool_t *parent_pool)
{
    apr_pool_t   *pool;
    apr_status_t  rv;

    CHECK(apr_pool_create(&pool, parent_pool));
    *iobuf = apr_pcalloc(pool, sizeof(cpe_io_buf));
    if (*iobuf == NULL) {
        cpe_log(CPE_ERR, "%s", "out of memory");
        apr_pool_destroy(pool);
        return APR_EGENERAL;
    }
    (*iobuf)->buf = apr_pcalloc(pool, bufsize);
    if ((*iobuf)->buf == NULL) {
        cpe_log(CPE_ERR, "%s", "out of memory");
        apr_pool_destroy(pool);
        return APR_EGENERAL;
    }
    (*iobuf)->buf_capacity = bufsize;
    (*iobuf)->pool = pool;
    cpe_log(CPE_DEB, "created iobuf %p, iobuf->buf %p", *iobuf, (*iobuf)->buf);
    return APR_SUCCESS;
}


/**
 * Since apr_pool_destroy() is void, it should always succeed.
 */
void
cpe_iobuf_destroy(cpe_io_buf **iobuf, cpe_network_ctx *nctx)
{
    assert((*iobuf)->pool != NULL);
    cpe_log(CPE_DEB, "destroying iobuf %p, pool %p", *iobuf, (*iobuf)->pool);
    apr_pool_destroy((*iobuf)->pool);
    *iobuf = NULL;
    if (nctx != NULL) {
        /* very important for cpe_receiver() */
        nctx->nc_iobuf = NULL;
    }
}


/** Init \p iobuf (already existing), allocating \p bufsize bytes from \p pool.
 */
apr_status_t
cpe_iobuf_init(cpe_io_buf *iobuf, int bufsize, apr_pool_t *pool)
{
    memset(iobuf, 0, sizeof *iobuf);
    iobuf->buf = apr_pcalloc(pool, bufsize);
    if (iobuf->buf == NULL) {
        return APR_EGENERAL;
    }
    iobuf->buf_capacity = bufsize;
    return APR_SUCCESS;
}


/*
 * CPE internal usage only.
 */
apr_status_t
cpe_network_init(apr_pool_t *pool)
{
    apr_status_t rv;
    int          flags = 0;
    int          port = 0;

    rv = apr_sockaddr_info_get(&g_cpe_sockaddr_localhost, "127.0.0.1",
        APR_INET, port, flags, pool);
    return rv;
}
