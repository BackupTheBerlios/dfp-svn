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

#include "cpe.h"
#include "apr_strings.h"

/** Like apr_strerror(), but doesn't require to provide a buffer. This also
 *  means that this function is non reentrant.
 */
const char *
cpe_errmsg(apr_status_t retvalue)
{
    static char buf[80];
    static char errmsg[60];

    if (retvalue == APR_SUCCESS) {
        return "success";
    } else {
        apr_strerror(retvalue, errmsg, sizeof errmsg);
        apr_snprintf(buf, sizeof buf, "%s (native: %d)", errmsg,
            APR_TO_OS_ERROR(retvalue));
        return buf;
    }
}


/** Transform msec in usec. Since APR doesn't have it, we provide our own.
 */
apr_time_t
cpe_time_from_msec(int msec)
{
    return msec * 1000;
}


/** Transform hour in usec. Since APR doesn't have it, we provide our own.
 */
apr_time_t
cpe_time_from_hour(int hour)
{
    return apr_time_from_sec(hour * 60 * 60);
}
