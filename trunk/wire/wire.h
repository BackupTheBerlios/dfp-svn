/*
 * Portable implementation of the Dynamic Feedback Protocol (DFP)
 *
 * $Id: wire.h 136 2007-03-27 12:27:01Z mmolteni $
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

#ifndef DFP_WIRE_INCLUDED
#define DFP_WIRE_INCLUDED

#include <sys/types.h>
#include <apr_errno.h>
#include "cpe-network.h"
#include "cpe-logging.h"


/** @defgroup dfp_wire DFP (Dynamic Feedback Protocol) wire protocol and format

 All messages are assumed to be in network byte order.

 Message format:
 <pre>
 +-----------------------+
 |     Message Header    |
 +-----------------------+
 |      First TLV        |
 +-----------------------+
 |     Second TLV        |
 +-----------------------+
 |         .....         |
 +-----------------------+
 |      Last TLV         |
 +-----------------------+
 </pre>

 @{
 */


#define DFP_MSG_VERSION_1    0x01

#define DFP_MSG_PREF_INFO    0x0101
#define DFP_MSG_SERVER_STATE 0x0201
#define DFP_MSG_DFP_PARAMS   0x0301
#define DFP_MSG_BIND_REQ     0x0401
#define DFP_MSG_BIND_REPORT  0x0402
/* Note: draft-01 has the same value 0x0402 for both DFP_MSG_BIND_REPORT and
 * DFP_MSG_BIND_CHANGE
 */
#define DFP_MSG_BIND_CHANGE  0x0403
/*
 * Customer Private Use   0x0500-0x05FF
 */

/* The smaller message is a DFP_MSG_PREF_INFO with empty payload. */
#define DFP_MIN_MSG_SIZE sizeof(dfp_msg_header_t)


struct dfp_msg_header {
    uint8_t  msg_version;
    uint8_t  msg_reserved1;
    uint16_t msg_type;
    uint32_t msg_len;
} __attribute__((packed));
typedef struct dfp_msg_header dfp_msg_header_t;

#define DFP_TLV_SECURITY      0x0001
#define DFP_TLV_LOAD          0x0002
#define DFP_TLV_KEEPALIVE     0x0101
/* Reserved: 0x0200-0x02ff */
#define DFP_TLV_BIND_ID_TABLE 0x0301

struct dfp_tlv_header {
    uint16_t tlv_type;
    uint16_t tlv_len;
} __attribute__((packed));
typedef struct dfp_tlv_header dfp_tlv_header_t;


/*
 * Security TLV
 */
#define DFP_MD5_SECURITY 0x00000001

/* XXX not sure this nesting makes sense for just one 32-bit int */
struct dfp_tlv_security {
    dfp_tlv_header_t sec_header;
    uint32_t         sec_algorithm;
} __attribute__((packed));
typedef struct dfp_tlv_security dfp_tlv_security_t;

struct dfp_tlv_security_md5 {
    uint32_t md5_key_id;
    uint8_t  md5_auth_data[16];
} __attribute__((packed));
typedef struct dfp_tlv_security_md5 dfp_tlv_security_md5_t;


/*
 * Load TLV
 */
#define DFP_LOAD_ANY_PORT  0
#define DFP_LOAD_ANY_PROTO 0

struct dfp_tlv_load_preference {
   uint32_t pref_ipaddr_v4;
   uint16_t pref_bind_id;
   uint16_t pref_weight;
} __attribute__((packed));
typedef struct dfp_tlv_load_preference dfp_tlv_load_preference_t;

struct dfp_tlv_load {
    dfp_tlv_header_t load_header;
    uint16_t         load_portn;
    uint8_t          load_protocol;
    uint8_t          load_flags;
    uint16_t         load_nhosts;
    uint16_t         load_reserved1;
/*
 *  one or more dfp_tlv_load_preference{}, one per host
 */
} __attribute__((packed));
typedef struct dfp_tlv_load dfp_tlv_load_t;


/*
 * KeepAlive TLV
 */

struct dfp_tlv_keepalive {
    dfp_tlv_header_t ka_header;
    uint32_t         ka_interval_sec;
} __attribute__((packed));
typedef struct dfp_tlv_keepalive dfp_tlv_keepalive_t;


/*
 * Bind Id Table TLV
 */

struct dfp_tlv_bind_id {
    uint16_t bid_id;
    uint16_t bid_reserved1;
    uint32_t bid_ipaddr_v4;
    uint32_t bid_netmask_v4;
} __attribute__((packed));
typedef struct dfp_tlv_bind_id dfp_tlv_bind_id_t;

struct dfp_tlv_bind_id_table {
    dfp_tlv_header_t btable_header;
    uint32_t         btable_ip_addr_v4;
    uint16_t         btable_port_n;
    uint8_t          btable_protocol;
    uint8_t          btable_reserved1;
    uint16_t         btable_entry_n;
    uint16_t         btable_reserved2;
/*
 *  zero or more dfp_tlv_bind_id{}, one per entry
 */
} __attribute__((packed));
typedef struct dfp_tlv_bind_id_table dfp_tlv_bind_id_table_t;



apr_status_t
dfp_msg_server_state_prepare(cpe_io_buf *iobuf, int reqlen, int *start);
apr_status_t
dfp_msg_server_state_complete(cpe_io_buf *iobuf, int *start,
    apr_socket_t *sock, uint16_t bind_id, uint16_t weight);
apr_status_t
dfp_msg_bind_report_prepare(cpe_io_buf *iobuf, int reqlen, int *start);
apr_status_t
dfp_msg_pref_info_prepare(cpe_io_buf *iobuf, int reqlen, int *start);
apr_status_t
dfp_msg_pref_info_complete(cpe_io_buf *iobuf, int *start,
    apr_socket_t *sock, uint16_t bind_id, uint16_t weight);
apr_status_t
dfp_msg_bind_change_prepare(cpe_io_buf *iobuf);
apr_status_t
dfp_msg_dfp_parameters_complete(cpe_io_buf *iobuf, int *start,
    uint32_t interval_sec);
apr_status_t
dfp_msg_bind_req_complete(cpe_io_buf *iobuf);

apr_status_t
dfp_msg_tlv_bind_table_prepare(cpe_io_buf *iobuf, uint32_t ip_addr_v4,
    uint16_t port_n, uint8_t protocol, uint16_t entry_n);
apr_status_t
dfp_tlv_keepalive_prepare(cpe_io_buf *iobuf, uint32_t interval_sec);
apr_status_t
dfp_tlv_load_prepare(cpe_io_buf *iobuf, uint portn, uint proto,
    uint flags, uint nhosts);
apr_status_t
dfp_tlv_load_add_hostpref(cpe_io_buf *iobuf, uint32_t ipaddr_v4,
    uint16_t bind_id, uint16_t weight);

char *
dfp_msg_type2string(uint16_t type);


/* @} */
#endif /* DFP_WIRE_INCLUDED */
