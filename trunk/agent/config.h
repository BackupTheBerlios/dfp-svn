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

#ifndef DFP_CONFIG_INCLUDED
#define DFP_CONFIG_INCLUDED

#include "apr_time.h"
#include "apr_strings.h"
#include "cpe-logging.h"

/* NOTE Default port according to IETF draft is 8080. */
#define DFP_CFG_LISTEN_PORT         8081
#define DFP_CFG_LISTEN_ADDRESS      "127.0.0.1"
#define DFP_CFG_LOG_LEVEL           CPE_INFO
#define DFP_CFG_LOOP_DURATION       0
#define DFP_CFG_KEEPALIVE_INTERVAL  apr_time_from_sec(5)

struct dfp_config_t {
    int        dc_listen_port;
    char       dc_listen_address[80];
    int        dc_log_level;
    apr_time_t dc_loop_duration;
    apr_time_t dc_keepalive_interval;
};
typedef struct dfp_config_t dfp_config_t;

apr_status_t dfp_agent_config(dfp_config_t *config,
    int argc, const char *const *argv);
apr_status_t dfp_manager_config(dfp_config_t *config,
    int argc, const char *const *argv);

#endif /* DFP_CONFIG_INCLUDED */
