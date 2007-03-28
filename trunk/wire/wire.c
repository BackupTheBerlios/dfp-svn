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

#include <assert.h>
#include <netinet/in.h>
#include "wire.h"


static apr_status_t
dfp_msg_header_prepare(cpe_io_buf *iobuf, int reqlen, uint16_t msg_type,
    int *start);


/*****************************************************************************
 *                                 INTERFACE                                 *
 *****************************************************************************/


/*
 * Messages
 */


/** SERVER_STATE message (agent <== manager)
 *
 * @param iobuf  iobuf containing one or more DFP messages.
 * @param start  will be set to the start of this message.
 * @param reqlen length of this message.
 */
apr_status_t
dfp_msg_server_state_prepare(cpe_io_buf *iobuf, int reqlen, int *start)
{
    apr_status_t rv;

    CHECK(dfp_msg_header_prepare(iobuf, reqlen, DFP_MSG_SERVER_STATE, start));
    return APR_SUCCESS;
}


/** SERVER_STATE message (agent <== manager)
 */
apr_status_t
dfp_msg_server_state_complete(cpe_io_buf *iobuf, int *start,
    apr_socket_t *sock, uint16_t bind_id, uint16_t weight)
{
    apr_status_t    rv;
    apr_sockaddr_t *sockaddr;
    uint32_t       *ipaddr_v4;
    uint            flags, nhosts;
    int             reqlen;

    /* Extract IP address from the socket. */
    CHECK(apr_socket_addr_get(&sockaddr, APR_LOCAL, sock));
    ipaddr_v4 = (uint32_t *) sockaddr->ipaddr_ptr;

    /* Build msg header and Load TLV for 1 host */
    nhosts = 1;
    reqlen = sizeof(dfp_msg_header_t) + sizeof(dfp_tlv_load_t) + \
        nhosts * sizeof(dfp_tlv_load_preference_t);
    CHECK(dfp_msg_server_state_prepare(iobuf, reqlen, start));
    flags = 0; /* unused */
    CHECK(dfp_tlv_load_prepare(iobuf, DFP_LOAD_ANY_PORT, DFP_LOAD_ANY_PROTO,
        flags, nhosts));

    /* Build hostpref TLV */
    CHECK(dfp_tlv_load_add_hostpref(iobuf, *ipaddr_v4, bind_id, weight));

    return APR_SUCCESS;
}


/** BIND_REPORT message (agent ==> manager)
 *
 * @param iobuf  iobuf containing one or more DFP messages.
 * @param start  will be set to the start of this message.
 * @param reqlen length of this message.
 */
apr_status_t
dfp_msg_bind_report_prepare(cpe_io_buf *iobuf, int reqlen, int *start)
{
    apr_status_t rv;

    CHECK(dfp_msg_header_prepare(iobuf, reqlen, DFP_MSG_BIND_REPORT, start));
    /* XXX WRITEME */
    return APR_SUCCESS;
}


/** PREF_INFO message (agent => manager)
 *
 * @param iobuf  iobuf containing one or more DFP messages.
 * @param start  will be set to the start of this message.
 * @param reqlen message length.
 */
apr_status_t
dfp_msg_pref_info_prepare(cpe_io_buf *iobuf, int reqlen, int *start)
{
    apr_status_t rv;

    CHECK(dfp_msg_header_prepare(iobuf, reqlen, DFP_MSG_PREF_INFO, start));
    return APR_SUCCESS;
}


/** PREF_INFO message (agent => manager)
 *
 * @param iobuf  iobuf containing one or more DFP messages.
 * @param start  will be set to the start of this message.
 * @param reqlen length of this message.
 */
apr_status_t
dfp_msg_pref_info_complete(cpe_io_buf *iobuf, int *start,
    apr_socket_t *sock, uint16_t bind_id, uint16_t weight)
{
    apr_status_t    rv;
    apr_sockaddr_t *sockaddr;
    uint32_t       *ipaddr_v4;
    uint            flags, nhosts;
    int             reqlen;

    /* Extract IP address from the socket. */
    CHECK(apr_socket_addr_get(&sockaddr, APR_LOCAL, sock));
    ipaddr_v4 = (uint32_t *) sockaddr->ipaddr_ptr;

    /* Build msg header and Load TLV for 1 host */
    nhosts = 1;
    reqlen = sizeof(dfp_msg_header_t) + sizeof(dfp_tlv_load_t) + \
        nhosts * sizeof(dfp_tlv_load_preference_t);
    CHECK(dfp_msg_pref_info_prepare(iobuf, reqlen, start));
    flags = 0; /* unused */
    CHECK(dfp_tlv_load_prepare(iobuf, DFP_LOAD_ANY_PORT, DFP_LOAD_ANY_PROTO,
        flags, nhosts));

    /* Build hostpref TLV */
    CHECK(dfp_tlv_load_add_hostpref(iobuf, *ipaddr_v4, bind_id, weight));

    return APR_SUCCESS;
}


/** BIND_CHANGE_NOTIFY message (agent ==> manager)
 *  Size is fixed for this kind of message.
 */
apr_status_t
dfp_msg_bind_change_prepare(cpe_io_buf *iobuf)
{
    apr_status_t rv;
    int          reqlen;
    int          start = 0;

    reqlen = sizeof(dfp_msg_header_t);
    CHECK(dfp_msg_header_prepare(iobuf, reqlen, DFP_MSG_BIND_CHANGE, &start));
    return APR_SUCCESS;
}


/** DFP_PARAMS message (agent <== manager)
 */
apr_status_t
dfp_msg_dfp_parameters_complete(cpe_io_buf *iobuf, int *start,
    uint32_t interval_sec)
{
    apr_status_t rv;
    int          reqlen;

    reqlen = sizeof(dfp_msg_header_t) + sizeof(dfp_tlv_keepalive_t);
    CHECK(dfp_msg_header_prepare(iobuf, reqlen, DFP_MSG_DFP_PARAMS, start));
    CHECK(dfp_tlv_keepalive_prepare(iobuf, interval_sec));
    return APR_SUCCESS;
}


/** DFP_BIND_REQ message (agent <== manager) */
apr_status_t
dfp_msg_bind_req_complete(cpe_io_buf *iobuf)
{
    apr_status_t rv;
    int          reqlen;
    int          start = 0;

    reqlen = sizeof(dfp_msg_header_t);
    CHECK(dfp_msg_header_prepare(iobuf, reqlen, DFP_MSG_BIND_REQ, &start));
    return APR_SUCCESS;
}


/*
 * TLVs
 */


apr_status_t
dfp_msg_tlv_bind_table_prepare(
    cpe_io_buf *iobuf,
    uint32_t    ip_addr_v4,
    uint16_t    port_n,
    uint8_t     protocol,
    uint16_t    entry_n)
{
    int                      reqlen, avail;
    dfp_tlv_bind_id_table_t *tlv;

    reqlen = sizeof(dfp_tlv_bind_id_table_t) + \
        entry_n * sizeof(dfp_tlv_bind_id_t);
    avail = iobuf->buf_capacity - iobuf->buf_len;
    if (reqlen > avail) {
        cpe_log(CPE_DEB, "not enough space (requested %d available %d)",
            reqlen, avail);
        return APR_EGENERAL;
    }

    tlv = (dfp_tlv_bind_id_table_t *) &iobuf->buf[iobuf->buf_len];
    tlv->btable_ip_addr_v4 = ip_addr_v4;
    tlv->btable_port_n     = htons(port_n);
    tlv->btable_protocol   = protocol;
    tlv->btable_reserved1  = 0;
    tlv->btable_entry_n    = htons(entry_n);
    tlv->btable_reserved2  = 0;

    iobuf->buf_len += sizeof(dfp_tlv_bind_id_table_t);
    return APR_SUCCESS;
}


apr_status_t
dfp_tlv_keepalive_prepare(cpe_io_buf *iobuf, uint32_t interval_sec)
{
    int                  reqlen, avail;
    dfp_tlv_keepalive_t *tlv;

    reqlen = sizeof(dfp_tlv_keepalive_t);
    avail = iobuf->buf_capacity - iobuf->buf_len;
    if (reqlen > avail) {
        cpe_log(CPE_DEB, "not enough space (requested %d available %d)",
            reqlen, avail);
        return APR_EGENERAL;
    }

    tlv = (dfp_tlv_keepalive_t *) &iobuf->buf[iobuf->buf_len];
    tlv->ka_header.tlv_type = htons(DFP_TLV_KEEPALIVE);
    tlv->ka_header.tlv_len  = htons(reqlen);
    tlv->ka_interval_sec    = htonl(interval_sec);

    iobuf->buf_len += sizeof(dfp_tlv_keepalive_t);

    return APR_SUCCESS;
}

/** Prepare a DFP_TLV_LOAD TLV, containing \p nhosts entries.
 *  Assumes that \p iobuf has been properly set by dfp_msg_pref_info_prepare().
 */
apr_status_t
dfp_tlv_load_prepare(cpe_io_buf *iobuf, uint portn, uint proto,
    uint flags, uint nhosts)
{
    int               reqlen, avail;
    dfp_tlv_load_t   *tlv;

    reqlen = sizeof(dfp_tlv_load_t) + nhosts * sizeof(dfp_tlv_load_preference_t);
    avail = iobuf->buf_capacity - iobuf->buf_len;
    if (reqlen > avail) {
        cpe_log(CPE_DEB, "not enough space (requested %d available %d)",
            reqlen, avail);
        return APR_EGENERAL;
    }

    tlv = (dfp_tlv_load_t *) &iobuf->buf[iobuf->buf_len];
    tlv->load_header.tlv_type = htons(DFP_TLV_LOAD);
    tlv->load_header.tlv_len  = htons(reqlen);
    tlv->load_portn           = htons(portn);
    tlv->load_protocol        = proto;
    tlv->load_flags           = flags;
    tlv->load_nhosts          = htons(nhosts);

    iobuf->buf_len += sizeof(dfp_tlv_load_t);

    return APR_SUCCESS;
}


/** Add a Host Preference to a Load TLV.
 *  @param ipaddr_v4 Assumed to be already in network byte order.
 *  @bug using an uint32_t for ipaddr_v4 storage is too low level; need to replace
 *       with something better from APR.
 */
apr_status_t
dfp_tlv_load_add_hostpref(cpe_io_buf *iobuf, uint32_t ipaddr_v4,
    uint16_t bind_id, uint16_t weight)
{
    int reqlen, avail;
    dfp_tlv_load_preference_t *lpref;

    reqlen = sizeof(dfp_tlv_load_preference_t);
    avail = iobuf->buf_capacity - iobuf->buf_len;
    if (reqlen > avail) {
        cpe_log(CPE_DEB, "not enough space (requested %d available %d)",
            reqlen, avail);
        return APR_EGENERAL;
    }

    lpref = (dfp_tlv_load_preference_t *) &iobuf->buf[iobuf->buf_len];
    lpref->pref_ipaddr_v4 = ipaddr_v4;
    lpref->pref_bind_id   = htons(bind_id);
    lpref->pref_weight    = htons(weight);

    iobuf->buf_len += sizeof(dfp_tlv_load_preference_t);

    return APR_SUCCESS;
}


char *
dfp_msg_type2string(uint16_t type)
{
    char *str;

    switch (type) {

    case DFP_MSG_PREF_INFO:
        str = "Preference Info";
    break;
    case DFP_MSG_SERVER_STATE:
        str = "Server State";
    break;
    case DFP_MSG_DFP_PARAMS:
        str = "DFP Parameters";
    break;
    case DFP_MSG_BIND_REQ:
        str = "BindId Request";
    break;
    case DFP_MSG_BIND_REPORT:
        str = "BindId Table Report";
    break;
    case DFP_MSG_BIND_CHANGE:
        str = "BindId Change Notify";
    break;
    default:
        str = "Unknown";
    }
    return str;
}


/*****************************************************************************
 *                               IMPLEMENTATION                              *
 *****************************************************************************/


static apr_status_t
dfp_msg_header_prepare(cpe_io_buf *iobuf, int reqlen, uint16_t msg_type,
    int *start)
{
    dfp_msg_header_t *msg_hdr;
    int               avail;

    *start = 0;
    if (reqlen < (int) sizeof(dfp_msg_header_t)) {
        cpe_log(CPE_DEB, "requested (%d) less than minumum", reqlen);
        return APR_EINVAL;
    }
    avail = iobuf->buf_capacity - iobuf->buf_len;
    if (reqlen > avail) {
        cpe_log(CPE_DEB, "not enough space (requested %d available %d)",
            reqlen, avail);
        return APR_EGENERAL;
    }
    *start = iobuf->buf_len;
    msg_hdr = (dfp_msg_header_t *) &iobuf->buf[*start];
    msg_hdr->msg_version   = DFP_MSG_VERSION_1;
    msg_hdr->msg_reserved1 = 0;
    msg_hdr->msg_type      = htons(msg_type);
    msg_hdr->msg_len       = htonl(reqlen);

    /* buf_len MUST be incremented step by step by the tlv constructors. */
    iobuf->buf_len += sizeof(dfp_msg_header_t);

    return APR_SUCCESS;
}
