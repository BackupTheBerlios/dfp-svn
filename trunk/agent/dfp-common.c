/*
 * Portable implementation of the Dynamic Feedback Protocol (DFP)
 *
 * $Id: dfp-common.c 136 2007-03-27 12:27:01Z mmolteni $
 *
 * Code that can be shared between the agent and the manager.
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

#include "dfp-common.h"
#include "wire.h"

/* This can be either a
 * agent -> manager    Preference Info msg
 * or
 * agent <- manager    Server Status msg
 */
apr_status_t
dfp_parse_load(cpe_io_buf *iobuf, int payload_offset, int payload_len,
    uint32_t *pref_ipaddr_v4, uint16_t *pref_bind_id, uint16_t *pref_weight)
{
    int                        nhosts;
    int                        minlen;
    dfp_tlv_load_t            *load_tlv;
    dfp_tlv_load_preference_t *load_pref;
    uint16_t                   tlv_type;
    uint16_t                   tlv_len;

    nhosts = 1;
    minlen = sizeof(dfp_tlv_load_t) + \
        nhosts * sizeof(dfp_tlv_load_preference_t);
    if (payload_len < minlen) {
        cpe_log(CPE_WARN, "payload len (%d) < minimum len (%d)",
            payload_len, minlen);
        return APR_EGENERAL;
    }

    load_tlv = (dfp_tlv_load_t *) &iobuf->buf[payload_offset];
    tlv_type = ntohs(load_tlv->load_header.tlv_type);
    tlv_len = ntohs(load_tlv->load_header.tlv_len);
    if (tlv_type != DFP_TLV_LOAD) {
        cpe_log(CPE_WARN, "TLV type (%#x) is not Load", tlv_type);
        return APR_EGENERAL;
    }
    if (tlv_len > payload_len) {
        cpe_log(CPE_WARN, "TLV len (%d) > payload len (%d)", tlv_len,
            payload_len);
        return APR_EGENERAL;
    }

    load_pref = (dfp_tlv_load_preference_t *) \
        &iobuf->buf[payload_offset + sizeof(dfp_tlv_load_t)];
    *pref_ipaddr_v4 = load_pref->pref_ipaddr_v4;
    *pref_bind_id   = ntohs(load_pref->pref_bind_id);
    *pref_weight    = ntohs(load_pref->pref_weight);

    return APR_SUCCESS;
}
