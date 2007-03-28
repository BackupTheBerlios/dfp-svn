/*
 * Portable implementation of the Dynamic Feedback Protocol (DFP)
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

#include "dfp.h"
#include "wire.h"
#include "config.h"
#include "dfp-common.h"
#include "cpe.h"
#include "cpe-network.h"


typedef struct timer_ctx_ {
    cpe_io_buf      *tc_iobuf;
    cpe_network_ctx *tc_nctx;
} timer_ctx_t;

static dfp_config_t   g_dfp_conf;
static apr_pool_t    *g_dfp_pool;

static int g_msg_types[] = {
    /* messages from manager to agent */
    DFP_MSG_SERVER_STATE,
    DFP_MSG_DFP_PARAMS,
    DFP_MSG_BIND_REQ,

    /* messages from agent to manager */
    DFP_MSG_PREF_INFO,
    DFP_MSG_BIND_REPORT,
    DFP_MSG_BIND_CHANGE,
};
#define NMESSAGES (sizeof(g_msg_types) / sizeof(g_msg_types[0]))

static apr_status_t client_cb(void *ctx, apr_pollfd_t *pfd, cpe_event *e);
static apr_status_t client_one_shot_cb(void *context, apr_pollfd_t *pfd,
    cpe_event *event);


int
main(int argc, const char *const *argv, const char *const *env)
{
    apr_status_t        rv;
    apr_socket_t       *csock;
    apr_sockaddr_t     *csockaddr;
    cpe_network_ctx    *client_ctx;

    /* Misc init.
     */
    CHECK(apr_app_initialize(&argc, &argv, &env));
    CHECK(apr_pool_create(&g_dfp_pool, NULL));
    CHECK(dfp_manager_config(&g_dfp_conf, argc, argv));
    CHECK(cpe_log_init(g_dfp_conf.dc_log_level));
    CHECK(cpe_system_init(CPE_NUM_EVENTS_DEFAULT));

    /* Network init.
     */
    client_ctx = apr_pcalloc(g_dfp_pool, sizeof *client_ctx);
    client_ctx->nc_pool = g_dfp_pool;
    CHECK(cpe_socket_client_create(&csock, &csockaddr,
        g_dfp_conf.dc_listen_address, g_dfp_conf.dc_listen_port, g_dfp_pool));
    CHECK(cpe_socket_after_connect(csock, csockaddr, 0, client_cb,
        client_ctx, APR_POLLIN | APR_POLLOUT, client_one_shot_cb,
        client_ctx, g_dfp_pool));

    /* Event loop.
     */
    CHECK(cpe_main_loop(g_dfp_conf.dc_loop_duration));
    return 0;
}


/* Prepare a bare-bone message of type msg_type.
 */
static apr_status_t
msg_prepare(cpe_io_buf *iobuf, int msg_type, apr_socket_t *sock)
{
    apr_status_t    rv;
    int             reqlen = 0;
    int             start = 0;
    uint16_t        bind_id, weight;
    static uint32_t interval = 0;

    switch (msg_type) {

    /* Messages that should be sent from the manager:
     */

    case DFP_MSG_DFP_PARAMS:
        interval++; interval %= 10;
        CHECK(dfp_msg_dfp_parameters_complete(iobuf, &start, interval));
        /* done */
    break;

    case DFP_MSG_SERVER_STATE:
        bind_id = 0xf0ca;
        weight = 0;
        CHECK(dfp_msg_server_state_complete(iobuf, &start, sock, bind_id, weight));
        /* done */
    break;

    case DFP_MSG_BIND_REQ:
        CHECK(dfp_msg_bind_req_complete(iobuf));
    break;

    /* Messages that should NOT be sent from the manager:
     */

    case DFP_MSG_PREF_INFO:
        reqlen = sizeof(dfp_msg_header_t);
        CHECK(dfp_msg_pref_info_prepare(iobuf, reqlen, &start));
        /* done */
    break;

    case DFP_MSG_BIND_REPORT:
        /* Although this message requires TLVs inside, we don't add them,
         * since it is just a message that MUST be discarded by the agent.
         */
        reqlen = sizeof(dfp_msg_header_t);
        CHECK(dfp_msg_bind_report_prepare(iobuf, reqlen, &start));
        /* done */
    break;

    case DFP_MSG_BIND_CHANGE:
        CHECK(dfp_msg_bind_change_prepare(iobuf));
        /* done */
    break;

    default:
        return APR_EGENERAL;
    }
    return APR_SUCCESS;
}


/** Callback to periodically send various DFP messages
 *
 */
static apr_status_t
timer_cb(void *context, apr_pollfd_t *pfd, cpe_event *event)
{
    timer_ctx_t    *ctx = context;
    cpe_io_buf     *iobuf = ctx->tc_iobuf;
    apr_status_t    rv;
    apr_socket_t   *sock;
    static int      i = 0;

    cpe_log(CPE_DEB, "%s", "enter");
    /* XXX TODO randomize our next timer interval
     */
    cpe_event_add(event);
    pfd = NULL;

    if (iobuf->inqueue) {
        cpe_log(CPE_WARN, "iobuf %p still in queue, skipping", iobuf);
        return APR_SUCCESS;
    }
    /* Reuse buffer from the beginning. */
    iobuf->buf_len = 0;

    /* Select a "random" message to send.
     */
    i++; i %= NMESSAGES;
    /* XXX BUG should get the remote sockaddr, not the local socket */
    CHECK_NULL(sock, cpe_queue_get_socket(ctx->tc_nctx->nc_sendQ));
    CHECK(msg_prepare(iobuf, g_msg_types[i], sock));
    CHECK(cpe_send_enqueue(ctx->tc_nctx->nc_sendQ, iobuf));

    return APR_SUCCESS;
}


/* Init timer context.
 */
static apr_status_t
timer_init(timer_ctx_t **tctx, cpe_network_ctx *nctx, apr_pool_t *pool)
{
    apr_status_t  rv;
    timer_ctx_t  *t2;

    cpe_log(CPE_DEB, "%s", "enter");
    *tctx = NULL;
    t2 = apr_pcalloc(pool, sizeof(timer_ctx_t));
    t2->tc_nctx = nctx;
    CHECK(cpe_iobuf_create(&t2->tc_iobuf, ONE_SI_KILO, pool));
    *tctx = t2;
    return APR_SUCCESS;
}


/* Called once on the newly accepted socket. Perform the initialization
 * steps that cannot be done before.
 */
static apr_status_t
client_one_shot_cb(void *context, apr_pollfd_t *pfd, cpe_event *event)
{
    cpe_network_ctx *nctx = context;
    timer_ctx_t     *timer_ctx;
    apr_status_t     rv;
    apr_time_t       timer_interval;

    cpe_log(CPE_DEB, "%s", "enter");

    /* Install keepalive callback
     */
    CHECK(timer_init(&timer_ctx, nctx, nctx->nc_pool));
    timer_interval = apr_time_from_sec(2);
    CHECK_NULL(event,
        cpe_event_timer_create(timer_interval, timer_cb, timer_ctx));
    CHECK(cpe_event_add(event));

    CHECK(cpe_queue_init(&nctx->nc_sendQ, pfd, g_dfp_pool, event));

    return rv;
}


static apr_status_t
dfp_handle_msg_preference_info(cpe_io_buf *iobuf, int payload_offset,
    int payload_len)
{
    apr_status_t rv;
    uint32_t     pref_ipaddr_v4;
    uint16_t     pref_bind_id;
    uint16_t     pref_weight;
    char         ipaddr_str[20];

    CHECK(dfp_parse_load(iobuf, payload_offset, payload_len,
        &pref_ipaddr_v4, &pref_bind_id, &pref_weight));

    apr_snprintf(ipaddr_str, sizeof ipaddr_str, "%pA",
        (struct in_addr *) &pref_ipaddr_v4);
    cpe_log(CPE_INFO,
        "received Preference Info msg, server %s, bindId %d, weight %d",
        ipaddr_str, pref_bind_id, pref_weight);

    return APR_SUCCESS;
}


/** State machine to handle a received DFP message.
 * @remark We must deallocate the iobuf once done.
 * This is symmetrical to the agent state machine
 */
static apr_status_t
dfp_msg_handler_cb(cpe_io_buf *iobuf, cpe_network_ctx *nctx)
{
    dfp_msg_header_t *hdr;
    uint16_t          msg_type;
    int32_t           msg_len;
    int               payload_offset;
    int               payload_len;

    hdr = (dfp_msg_header_t *) &iobuf->buf[0];

    /* Version has already been tested by dfp_get_msg_size_cb(). */
    msg_type = ntohs(hdr->msg_type);
    msg_len = ntohl(hdr->msg_len);
    if (msg_len < 0) {
        msg_len = 0;
    }
    cpe_log(CPE_DEB, "received msg version %#x, type '%s' (%#x), len %u, "
        "iobuf len %d", hdr->msg_version, dfp_msg_type2string(msg_type),
        msg_type, msg_len, iobuf->buf_len);

    if (msg_len != iobuf->buf_len) {
        cpe_log(CPE_DEB,
            "lenght mismatch (declared %d, real %d), trying to proceed anyway",
            msg_len, iobuf->buf_len);
    }
    payload_offset = sizeof(dfp_msg_header_t);
    payload_len = cpe_min(msg_len, iobuf->buf_len) - payload_offset;

    switch (msg_type) {

    case DFP_MSG_PREF_INFO:
        dfp_handle_msg_preference_info(iobuf, payload_offset, payload_len);
    break;

    case DFP_MSG_BIND_REPORT:
        cpe_log(CPE_INFO, "%s", "received bind report");
    break;

    case DFP_MSG_BIND_CHANGE:
        /* here we should ask for a bind req */
    break;

    case DFP_MSG_SERVER_STATE:
    case DFP_MSG_DFP_PARAMS:
    case DFP_MSG_BIND_REQ:
        cpe_log(CPE_INFO, "Unexpected message (wrong direction)"
            " %#x (%s), discarding", msg_type, dfp_msg_type2string(msg_type));
    break;

    default:
        cpe_log(CPE_INFO, "Received unknown message %#x, discarding", msg_type);
    }

    cpe_log(CPE_DEB, "finished consuming iobuf %p, discarding", iobuf);
    cpe_iobuf_destroy(&iobuf, nctx);
    return APR_SUCCESS;
}


/** Extract the message size from the DFP header (performing also preliminary
 *  input validation).
 */
static int
dfp_get_msg_size_cb(cpe_io_buf *iobuf, int *msg_size)
{
    dfp_msg_header_t *hdr;

    *msg_size = 0;
    hdr = (dfp_msg_header_t *) &iobuf->buf[0];
    if (hdr->msg_version != DFP_MSG_VERSION_1) {
        cpe_log(CPE_INFO, "Unknown DFP message version %#x", hdr->msg_version);
        /* This will drop the connection. */
        return APR_EGENERAL;
    }
    *msg_size = ntohl(hdr->msg_len);
    cpe_log(CPE_DEB, "msg declared length %d", *msg_size);
    return APR_SUCCESS;
}


/** Event callback on the connected socket.
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
        /* XXX FIXME 2000 */
        rv = cpe_receiver(nctx, pfd, 2000, nctx->nc_pool,
            sizeof(dfp_msg_header_t), dfp_get_msg_size_cb, dfp_msg_handler_cb);
    }
    return rv;
}
