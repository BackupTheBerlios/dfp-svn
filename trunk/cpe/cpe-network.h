/*
 * Cisco Portable Events (CPE)
 * $Id: cpe-network.h 136 2007-03-27 12:27:01Z mmolteni $
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

#ifndef CPE_NETWORK_INCLUDED
#define CPE_NETWORK_INCLUDED
/*! @defgroup cpe_network CPE (Cisco Portable Events) network
 *  Provides functions and callbacks to perform non-blocking network I/O
 *  within the CPE system. Contains also some utility socket wrappers.
 *
 *  @{
 */
#include <apr_poll.h>
#include <apr_network_io.h>
#include "cpe.h"

/** Max number of peers per listening socket.
 *  @todo perform some scaling tests, I have no idea of how this performs.
 */
#define CPE_MAX_PEERS 1000

/** Main structure used for I/O. */
typedef struct cpe_io_buf_ cpe_io_buf;
struct cpe_io_buf_ {
    cpe_io_buf *next;
    int         inqueue;
    int         total;
    int         destroy; /* should cpe_sender destroy this buf once sent? */
    apr_pool_t *pool;
    char       *buf;
    int         buf_offset;
    int         buf_len;      /* actual length */
    int         buf_capacity;
};

struct cpe_queue_head_ {
    cpe_io_buf   *cq_next;
    cpe_io_buf   *cq_tail;
    int           cq_nelems;
    apr_pollfd_t *cq_pfd;
    int           cq_total_sent;
    int           cq_total_in_queue;
};
typedef struct cpe_queue_head_ cpe_queue_t;

/** Callback context.
 * @todo Transform the iobufs in pointers to iobufs!
 */
struct cpe_network_ctx {
    int          nc_count;
    int          nc_state;
    int          nc_msg_size;   /* set by cpe_get_msg_size_t */
    cpe_io_buf   nc_send;       /* transform or remove! */
    cpe_io_buf   nc_recv;       /* transform or remove! */
    cpe_io_buf  *nc_iobuf;      /* used by cpe_receiver() */
    int          nc_total_received;
    cpe_queue_t *nc_sendQ;
    apr_pool_t  *nc_pool;
    void        *nc_user_data;
};
typedef struct cpe_network_ctx cpe_network_ctx;

typedef struct cpe_socket_prepare_ctx_ cpe_socket_prepare_ctx;

typedef apr_status_t (* cpe_afilter_t)(apr_sockaddr_t *sockaddr);
typedef apr_status_t (* cpe_get_msg_size_t)(cpe_io_buf *iobuf, int *msg_len);
typedef apr_status_t (* cpe_handle_msg_t)(cpe_io_buf *iobuf,
                            cpe_network_ctx *nctx);

apr_status_t
cpe_socket_client_create(apr_socket_t **sock, apr_sockaddr_t **sockaddr,
    const char *hostname, apr_port_t port, apr_pool_t *pool);
apr_status_t
cpe_socket_server_create(apr_socket_t **sock, apr_sockaddr_t **sockaddr,
    const char *hostname, apr_port_t port, apr_int32_t backlog,
    apr_pool_t *pool);
apr_status_t
cpe_filter_any(apr_sockaddr_t *incoming);
apr_status_t
cpe_socket_after_accept(apr_socket_t *lsock,
    cpe_callback_t callback, void *ctx1, apr_int16_t pfd_flags,
    cpe_afilter_t afilter_cb, int max_peers, cpe_callback_t one_shot_cb,
    void *ctx2, apr_pool_t *pool);
apr_status_t
cpe_socket_after_connect(apr_socket_t *csock, apr_sockaddr_t *sockaddr,
    apr_time_t timeout_us, cpe_callback_t callback, void *context,
    apr_int16_t pfd_flags, cpe_callback_t one_shot_cb, void *one_shot_ctx,
    apr_pool_t *pool);
apr_status_t
cpe_send(apr_socket_t *sock, cpe_io_buf *sendbuf, apr_size_t *howmany);
apr_status_t
cpe_recv(apr_socket_t *sock, cpe_io_buf *recvbuf, apr_size_t *howmany);
apr_status_t
cpe_send_enqueue(cpe_queue_t *head, cpe_io_buf *sendbuf);
apr_status_t
cpe_sender(cpe_network_ctx *nctx);
apr_status_t
cpe_receiver(cpe_network_ctx *ctx, apr_pollfd_t *pfd, int iobufsize,
    apr_pool_t *pool, int fixed_len, cpe_get_msg_size_t get_msg_size_cb,
    cpe_handle_msg_t chunk_handler_cb);
apr_status_t
cpe_socket_close(apr_socket_t *sock);
apr_status_t
cpe_queue_init(cpe_queue_t **head, apr_pollfd_t *pfd, apr_pool_t *pool,
    cpe_event *event);
apr_socket_t *
cpe_queue_get_socket(cpe_queue_t *head);
apr_status_t
cpe_iobuf_init(cpe_io_buf *iobuf, int bufsize, apr_pool_t *pool);
apr_status_t
cpe_iobuf_create(cpe_io_buf **iobuf, int bufsize, apr_pool_t *parent_pool);
void
cpe_iobuf_destroy(cpe_io_buf **iobuf, cpe_network_ctx *nctx);


/* @} */
#endif /* CPE_NETWORK_INCLUDED */
