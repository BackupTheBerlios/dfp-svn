/*
 * Portable implementation of the Dynamic Feedback Protocol (DFP)
 *
 * $Id: agent.c 136 2007-03-27 12:27:01Z mmolteni $
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
#include "dfp-private.h"
#include "probe.h"
#include "wire.h"
#include "config.h"
#include "dfp-common.h"
#include "cpe.h"
#include "cpe-network.h"


#define DFP_MAX_KEEPALIVE_INTERVAL_SEC 60


typedef struct timer_ctx_ {
    cpe_io_buf      *tc_iobuf;
    cpe_network_ctx *tc_nctx;
} timer_ctx_t;


static dfp_config_t   g_dfp_conf;
static apr_pool_t    *g_dfp_pool;
static cpe_event     *g_dfp_keepalive_event;
static cpe_queue_t   *g_dfp_sendQ;

dfp_probe_ctx_t      *g_dfp_probe_ctx;
dfp_calc_average_t    g_dfp_probe_calc_average;


static apr_status_t dfp_server_cb(void *ctx, apr_pollfd_t *pfd, cpe_event *e);
static apr_status_t dfp_one_shot_cb(void *context, apr_pollfd_t *pfd,
    cpe_event *event);


int
main(int argc, const char *const *argv, const char *const *env)
{
    apr_status_t        rv;
    apr_socket_t       *lsock;
    apr_sockaddr_t     *lsockaddr;
    cpe_network_ctx    *server_ctx;
    int                 backlog, max_peers;

    /* Misc init.
     */
    CHECK(apr_app_initialize(&argc, &argv, &env));
    CHECK(apr_pool_create(&g_dfp_pool, NULL));
    CHECK(dfp_agent_config(&g_dfp_conf, argc, argv));
    CHECK(cpe_log_init(g_dfp_conf.dc_log_level));
    CHECK(cpe_system_init(CPE_NUM_EVENTS_DEFAULT));
    CHECK(dfp_probe_init(g_dfp_pool));

    /* Network init.
     */
    backlog = 1;
    max_peers = 1;
    CHECK(cpe_socket_server_create(&lsock, &lsockaddr,
        g_dfp_conf.dc_listen_address, g_dfp_conf.dc_listen_port, backlog,
        g_dfp_pool));
    server_ctx = apr_pcalloc(g_dfp_pool, sizeof *server_ctx);
    server_ctx->nc_pool = g_dfp_pool;
    CHECK(cpe_socket_after_accept(lsock, dfp_server_cb, server_ctx,
        APR_POLLIN, cpe_filter_any, max_peers, dfp_one_shot_cb, server_ctx,
        g_dfp_pool));

    /* Event loop.
     */
    CHECK(cpe_main_loop(g_dfp_conf.dc_loop_duration));
    return 0;
}


/** Callback to periodically send a DFP Preference Information message.
 * This acts as an application-level keepalive, required by the DFP specs.
 * The sending period can change depending on received msg DFP Parameters.
 */
static apr_status_t
dfp_keepalive_cb(void *context, apr_pollfd_t *pfd, cpe_event *event)
{
    timer_ctx_t    *ctx = context;
    cpe_io_buf     *iobuf = ctx->tc_iobuf;
    apr_status_t    rv;
    int             start;
    uint16_t        bind_id, weight;
    int32_t         value;
    apr_socket_t   *sock;

    cpe_log(CPE_DEB, "%s", "enter");
    cpe_event_add(event);
    pfd = NULL;

    if (iobuf->inqueue) {
        cpe_log(CPE_WARN, "iobuf %p still in queue, skipping", iobuf);
        return APR_SUCCESS;
    }
    /* Reuse buffer from the beginning. */
    iobuf->buf_len = 0;

    /* Build the message.
     */
    CHECK_NULL(sock, cpe_queue_get_socket(ctx->tc_nctx->nc_sendQ));
    bind_id = 0;

    /* XXX Having a global like g_dfp_probe_ctx is a bit grossy; should be redesigned */
    g_dfp_probe_calc_average(g_dfp_probe_ctx, &value);
    /* XXX should check if we lose data from 32 to 16 */
    weight = value;
    CHECK(dfp_msg_pref_info_complete(iobuf, &start, sock, bind_id, weight));

    CHECK(cpe_send_enqueue(ctx->tc_nctx->nc_sendQ, iobuf));

    return APR_SUCCESS;
}


/* Init keepalive context.
 */
static apr_status_t
dfp_keepalive_init(timer_ctx_t **tctx, cpe_network_ctx *nctx, apr_pool_t *pool)
{
    apr_status_t  rv;
    timer_ctx_t  *t2;

    cpe_log(CPE_DEB, "%s", "enter");
    *tctx = NULL;
    CHECK_NULL(t2, apr_pcalloc(pool, sizeof(timer_ctx_t)));
    t2->tc_nctx = nctx;
    CHECK(cpe_iobuf_create(&t2->tc_iobuf, ONE_SI_KILO, pool));
    *tctx = t2;
    return APR_SUCCESS;
}


/* This call is async wrt the keepalive callback, and since the new interval
 * might be smaller than the old, we cannot wait for the callback to pick up
 * the update.
 */
static apr_status_t
dfp_keepalive_set_interval(apr_time_t interval)
{
    apr_status_t rv;

    CHECK(cpe_event_remove(g_dfp_keepalive_event));
    CHECK(cpe_event_set_timeout(g_dfp_keepalive_event, interval));
    /* Force the _first_ expiration ASAP; next expirations will follow the
     * value of interval.
     */
    CHECK(cpe_event_add2(g_dfp_keepalive_event, 1));
    return APR_SUCCESS;
}


/* Called once on the newly accepted socket. Perform the initialization
 * steps that cannot be done before.
 */
static apr_status_t
dfp_one_shot_cb(void *context, apr_pollfd_t *pfd, cpe_event *event)
{
    cpe_network_ctx *nctx = context;
    timer_ctx_t     *keepalive_ctx;
    apr_status_t     rv;

    cpe_log(CPE_DEB, "%s", "enter");

    /* Install keepalive callback
     */
    CHECK(dfp_keepalive_init(&keepalive_ctx, nctx, nctx->nc_pool));
    CHECK_NULL(event,
        cpe_event_timer_create(g_dfp_conf.dc_keepalive_interval,
            dfp_keepalive_cb, keepalive_ctx));
    g_dfp_keepalive_event = event;
    CHECK(cpe_event_add(event));

    CHECK(cpe_queue_init(&nctx->nc_sendQ, pfd, g_dfp_pool, event));
    g_dfp_sendQ = nctx->nc_sendQ;
    return rv;
}


/* Server State is informational only; the agent MUST NOT change its notion
 * of server load based on its contents.
 */
static apr_status_t
dfp_handle_msg_server_state(cpe_io_buf *iobuf, int payload_offset,
    int payload_len)
{
    apr_status_t rv;
    uint32_t     pref_ipaddr_v4;
    uint16_t     pref_bind_id;
    uint16_t     pref_weight;
    char         ipaddr_str[20];

    CHECK(dfp_parse_load(iobuf, payload_offset, payload_len,
        &pref_ipaddr_v4, &pref_bind_id, &pref_weight));

    /* Just log the info, as required by the spec.
     */
    apr_snprintf(ipaddr_str, sizeof ipaddr_str, "%pA",
        (struct in_addr *) &pref_ipaddr_v4);
    cpe_log(CPE_INFO,
        "received Server State msg, server %s, bindId %d, weight %d",
        ipaddr_str, pref_bind_id, pref_weight);

    return APR_SUCCESS;
}


static apr_status_t
dfp_handle_msg_dfp_parameters(cpe_io_buf *iobuf, int payload_offset,
    int payload_len)
{
    int                  minlen;
    uint16_t             tlv_type;
    uint16_t             tlv_len;
    uint32_t             interval_sec;
    dfp_tlv_keepalive_t *keepalive_tlv;
    apr_status_t         rv;

    /* TODO should perform some rate-limiting before further processing.
     */

    minlen = sizeof(dfp_tlv_keepalive_t);
    if (payload_len < minlen) {
        cpe_log(CPE_WARN, "payload len (%d) < minimum len (%d)",
            payload_len, minlen);
        return APR_EGENERAL;
    }

    keepalive_tlv = (dfp_tlv_keepalive_t *) &iobuf->buf[payload_offset];
    tlv_type = ntohs(keepalive_tlv->ka_header.tlv_type);
    tlv_len = ntohs(keepalive_tlv->ka_header.tlv_len);
    if (tlv_type != DFP_TLV_KEEPALIVE) {
        cpe_log(CPE_WARN, "TLV type (%#x) is not keepalive interval", tlv_type);
        return APR_EGENERAL;
    }
    if (tlv_len > payload_len) {
        cpe_log(CPE_WARN, "TLV len (%d) > payload len (%d)", tlv_len,
            payload_len);
        return APR_EGENERAL;
    }
    interval_sec = ntohl(keepalive_tlv->ka_interval_sec);
    if (interval_sec == 0) {
        cpe_log(CPE_DEB, "%s", "keepalive interval 0: informational: "
            "manager will not close connection in case of no response");
        return APR_SUCCESS;
    }
    if (interval_sec > DFP_MAX_KEEPALIVE_INTERVAL_SEC) {
        cpe_log(CPE_DEB, "keepalive interval (%d sec) too big, using %d sec",
            interval_sec, DFP_MAX_KEEPALIVE_INTERVAL_SEC);
        interval_sec = DFP_MAX_KEEPALIVE_INTERVAL_SEC;
    }
    cpe_log(CPE_INFO, "setting keepalive interval to %d sec", interval_sec);
    CHECK(dfp_keepalive_set_interval(apr_time_from_sec(interval_sec)));

    return APR_SUCCESS;
}


static apr_status_t
dfp_handle_msg_bind_request(void)
{
    static cpe_io_buf *iobuf;
    apr_status_t       rv;
    int                reqlen, start;

    /* A BindId Request message triggers the sending of a BindId Report message.
     *
     * Design note: at this point, we can either prepare the message and send
     * it right away, or schedule this task for later via a timer event.
     * To keep it simple, we do all the work right now.
     */

    /* one-shot */
    if (iobuf == NULL) {
        CHECK(cpe_iobuf_create(&iobuf, ONE_SI_KILO, g_dfp_pool));
        cpe_log(CPE_DEB, "one-shot: created iobuf %p", iobuf);

        /* XXX BUG this works only if we create a separate event,
         * or if we extend the resource register to accept a generic
         * callback
         */
        //cpe_resource_register_user(sock, dtor);
    }
    if (iobuf->inqueue) {
        cpe_log(CPE_WARN, "iobuf %p still in queue, skipping", iobuf);
        return APR_SUCCESS;
    }
    /* Reuse buffer from the beginning. */
    iobuf->buf_len = 0;

    /* According to the specs, when there is no table to send, the BindId
     * Report msg must be sent with a BindId Table TLV set to zero.
     */
    reqlen = sizeof(dfp_msg_header_t) + sizeof(dfp_tlv_bind_id_table_t);
    CHECK(dfp_msg_bind_report_prepare(iobuf, reqlen, &start));
    CHECK(dfp_msg_tlv_bind_table_prepare(iobuf, 0, 0, 0, 0));

    if (g_dfp_sendQ == NULL) {
        cpe_log(CPE_ERR, "%s", "sendQ is NULL");
        return APR_EGENERAL;
    }
    CHECK(cpe_send_enqueue(g_dfp_sendQ, iobuf));

    return APR_SUCCESS;
}


/* If a DFP message contains a security TLV, it MUST be the first TLV in the
 * message.
 */
static apr_status_t
dfp_handle_security_tlv(cpe_io_buf *iobuf, int payload_offset,
    int payload_len)
{
    /* XXX WRITE ME!!! */
    iobuf = NULL;
    payload_offset = 0;
    payload_len = 0;
    return APR_SUCCESS;
}


/** State machine to handle a received DFP message.
 * @remark We must deallocate the iobuf once done, so we cannot use the CHECK
 *         macro.
 */
static apr_status_t
dfp_msg_handler_cb(cpe_io_buf *iobuf, cpe_network_ctx *nctx)
{
    dfp_msg_header_t *hdr;
    uint16_t          msg_type;
    int32_t           msg_len;
    int               payload_offset;
    int               payload_len;
    apr_status_t      rv = APR_SUCCESS;

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

    case DFP_MSG_SERVER_STATE:
        rv = dfp_handle_security_tlv(iobuf, payload_offset, payload_len);
        rv = dfp_handle_msg_server_state(iobuf, payload_offset, payload_len);
    break;

    case DFP_MSG_DFP_PARAMS:
        rv = dfp_handle_security_tlv(iobuf, payload_offset, payload_len);
        rv = dfp_handle_msg_dfp_parameters(iobuf, payload_offset, payload_len);
    break;

    case DFP_MSG_BIND_REQ:
        rv = dfp_handle_security_tlv(iobuf, payload_offset, payload_len);
        rv = dfp_handle_msg_bind_request();
    break;

    case DFP_MSG_PREF_INFO:
    case DFP_MSG_BIND_REPORT:
    case DFP_MSG_BIND_CHANGE:
        cpe_log(CPE_INFO, "Unexpected message (wrong direction)"
            " %#x (%s), discarding", msg_type, dfp_msg_type2string(msg_type));
    break;

    default:
        cpe_log(CPE_INFO, "Received unknown message %#x, discarding", msg_type);
    }

    cpe_log(CPE_DEB, "finished consuming iobuf %p, discarding", iobuf);
    cpe_iobuf_destroy(&iobuf, nctx);
    return rv;
}


/** Extract the message size from the DFP header (performing also preliminary
 *  input validation).
 *
 * @remark There is a general resync problem with protocols on top of a
 *         stream-oriented transport like TCP: if I don't find what I expected,
 *         how do I known where is the start of the next PDU ? For example in
 *         this case, if the version is not 1, then I have no guarantee at all
 *         that the header layout is the same, as so how can I calculate the
 *         length ? So what we do is to drop the connection.
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


/** Event callback on the accepted socket.
 */
static apr_status_t
dfp_server_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
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
