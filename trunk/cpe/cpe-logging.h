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

#ifndef CPE_LOGGING_INCLUDED
#define CPE_LOGGING_INCLUDED
/*! @defgroup cpe_logging CPE (Cisco Portable Events) logging
 * @{
 */

#include <stdarg.h>
#include <stdio.h>
#include <apr_general.h>

enum {
    CPE_LOG_FIRST = 0,
    CPE_DEB,
    CPE_INFO,
    CPE_WARN,
    CPE_ERR,
    CPE_SILENT,
    CPE_LOG_LAST
};

/* Using a macro allows to insert the name of the calling function */
#define cpe_log(level, fmt, ...) \
    cpe_log2(level, "%s: " fmt "\n", __FUNCTION__, __VA_ARGS__)

apr_status_t cpe_log_init(int min_level);
/* __attribute__ is GCC specific, so we might need a workaround for other
 * compilers, like:
 * #if !defined(__attribute__)
 * #define __attribute__(p)
 * #endif
 */
void         cpe_log2(int level, char const *fmt, ...)
                __attribute__ ((format (printf, 2, 3)));

/*! @} */

#endif /* CPE_LOGGING_INCLUDED */
