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

#include "cpe-logging.h"

/*!
 * \todo this should be a proper logging library, with in-memory circular
 * buffer and periodic write to disk, rate-limit, under debug flags...,
 * interface to system logger (syslog under unix, foo under windows, ...),
 * timestamp...
 */

static int   g_cpe_log_level = CPE_INFO;
//static char *g_cpe_loglevelstr[] = {
//    "[DEB]  ",
//    "[INFO] ",
//    "[WARN] ",
//    "[ERR]  "
//};

/** Init the logging system.
 *  @param min_level Minimum log level to report.
 */
apr_status_t
cpe_log_init(int min_level)
{
    if (min_level <= CPE_LOG_FIRST || min_level >= CPE_LOG_LAST) {
        return APR_EINVAL;
    }
    g_cpe_log_level = min_level;
    return APR_SUCCESS;
}


/** Do not use this function. Use cpe_log() instead.
 */
void
cpe_log2(int level, char const *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (level >= g_cpe_log_level && level < CPE_SILENT) {
        /** @bug I would like to use stderr, but tap(3) uses stdout, and
         *  using stderr means the two streams get out of sync. I am not
         *  sure that using stdout is enough.
         */
        vfprintf(stdout, fmt, ap);
    }
    va_end(ap);
}
