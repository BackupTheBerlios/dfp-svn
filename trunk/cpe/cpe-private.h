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

/*
 * WARNING This file is for CPE internal usage only.
 */
#ifndef CPE_PRIVATE_INCLUDED
#define CPE_PRIVATE_INCLUDED

#include "cpe.h"

/*! Event data structure.
 */
struct cpe_event {
    CPE_PRIORITYQ_HEADER
    int            ev_flags;
    apr_pollfd_t   ev_pollfd;
    apr_time_t     ev_timeout_us;
    cpe_callback_t ev_callback;
    void          *ev_ctx;
    apr_uint32_t   ev_magic;
};

extern apr_pool_t           *g_cpe_pool;

/* from cpe-network.c */
apr_status_t cpe_network_init(apr_pool_t *pool);

/* from cpe-resources.c */
apr_status_t cpe_resource_init(void);


#endif /* CPE_PRIVATE_INCLUDED */
