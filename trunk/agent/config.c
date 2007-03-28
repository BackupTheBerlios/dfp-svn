/*
 * Portable implementation of the Dynamic Feedback Protocol (DFP)
 *
 * $Id: config.c 136 2007-03-27 12:27:01Z mmolteni $
 *
 * This file is a quick hack for all configurable settings, that will go
 * away once we find a portable, simple, working configuration file parsing
 * library. Among the candidates: YAML in C.
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

#include <stdlib.h>
#include "apr_getopt.h"
#include "config.h"

/*
 * TODO parse file
 */
static apr_status_t
dfp_config_from_file(dfp_config_t *config)
{
    config->dc_listen_port        = DFP_CFG_LISTEN_PORT;
    apr_cpystrn(config->dc_listen_address, DFP_CFG_LISTEN_ADDRESS,
        sizeof config->dc_listen_address);
    config->dc_log_level          = DFP_CFG_LOG_LEVEL;
    config->dc_loop_duration      = DFP_CFG_LOOP_DURATION;
    config->dc_keepalive_interval = DFP_CFG_KEEPALIVE_INTERVAL;

    return APR_SUCCESS;
}

static apr_status_t
dfp_config_from_command_line(dfp_config_t *config,
    int argc, const char *const *argv)
{
    apr_status_t  rv;
    apr_pool_t   *pool;
    apr_getopt_t *opt;
    int           optch;
    const char   *optarg;
    int           i;
    static const apr_getopt_option_t options[] = {
        /* long-option, short-option, has-arg flag, description */
        { "address", 'a', TRUE,  "listen address"                  },
        { "debug",   'd', TRUE,  "debug level"                     },
        { "port",    'p', TRUE,  "listen port"                     },
        { "timeout", 't', TRUE,  "main loop duration [sec]"        },
        { NULL,       0,  0,     NULL                              } /* end */
    };

    apr_pool_create(&pool, NULL);
    apr_getopt_init(&opt, pool, argc, argv);

    while ((rv = apr_getopt_long(opt, options, &optch, &optarg)) == APR_SUCCESS) {
        switch (optch) {
        case 'a':
            apr_cpystrn(config->dc_listen_address, optarg,
                sizeof config->dc_listen_address);
            break;
        case 'd':
            config->dc_log_level = atoi(optarg);
            break;
        case 'p':
            config->dc_listen_port = atoi(optarg);
            break;
        case 't':
            config->dc_loop_duration = apr_time_from_sec(atoi(optarg));
            break;
        }
    }
    if (rv == APR_BADCH) {
        printf("usage: %s [opts]\n", argv[0]);
        for (i = 0; options[i].name != NULL; i++) {
            printf("-%c %s\n", options[i].optch, options[i].description);
        }
    }
    apr_pool_destroy(pool);
    if (rv == APR_EOF) {
        return APR_SUCCESS;
    }
    return rv;
}

/** Read the configuration file and initialize the associated data.
 */
apr_status_t
dfp_agent_config(dfp_config_t *config, int argc, const char *const *argv)
{
    apr_status_t rv;

    if ((rv = dfp_config_from_file(config)) != APR_SUCCESS) {
        return rv;
    }
    /* And now override with command-line parameters. */
    return dfp_config_from_command_line(config, argc, argv);
}
