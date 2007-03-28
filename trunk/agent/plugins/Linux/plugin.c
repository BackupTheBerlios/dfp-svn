/*
 * Portable implementation of the Dynamic Feedback Protocol (DFP)
 *
 * $Id$
 *
 * Linux-specific probe.
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
#include "cpe.h"
#include "cpe-logging.h"

#include <stdlib.h>


/* Probe context; might be useful or not depending on the specific
 * implementation.
 */
static struct lnx_probe_ctx_ {
    int p_foo;
} lnx_probe_ctx;
typedef struct lnx_probe_ctx_ lnx_probe_ctx_t;


static apr_status_t
lnx_probe_take_measure(void *context, int *value);
static apr_status_t
lnx_probe_calc_average(dfp_probe_ctx_t *ctx, int *value);


/*****************************************************************************
 *                                 INTERFACE                                 *
 *****************************************************************************/

/* Entry point in the DFP plugin system.
 */
apr_status_t
plugin_init(
    char               **probe_name,
    apr_time_t          *poll_interval,
    dfp_take_measure_t  *probe_take_measure,
    void               **take_measure_ctx,
    dfp_calc_average_t  *probe_calc_average)
{
    *probe_name         = "linux plugin";
    *poll_interval      = apr_time_from_sec(5);
    *probe_take_measure = lnx_probe_take_measure;
    *take_measure_ctx   = &lnx_probe_ctx;

    /* Overriding this is not normally needed, since DFP knows how to calculate
     * a moving average for you. Use it only if you know what you are doing.
     */
    *probe_calc_average = lnx_probe_calc_average;

    return APR_SUCCESS;
}


/*****************************************************************************
 *                               IMPLEMENTATION                              *
 *****************************************************************************/


/* Called periodically by the DFP probe subsystem.
 *
 * Here a real probe would gather a snapshot of the quantity it is due to
 * measure (CPU load, free memory, whatever) and return it in \p value.
 *
 * NOTE The DFP specs don't specify the weight range; they specify only that
 * a weight of 0 means full load, i.e. that server is not available for any
 * more flows.
 *
 * For the time being we use the convention that our values have a range
 * from 0 to 100, where 0 means full load and 100 means 0 load.
 */
static apr_status_t
lnx_probe_take_measure(void *context, int *value)
{
    lnx_probe_ctx_t *ctx = context;

    ctx = NULL;
    *value = 0;

    return APR_SUCCESS;
}


/* WARNING: Normally you do NOT need to provide this method, DFP does it for
 * you.
 *
 * XXX WARNING The load average is NOT a good measure of system availability
 * for example, if a process is swapping to disk, load average will remain
 * low but the system will be unresponsive!!!!
 *
 * In this particular case, we want to override it because the OS keeps a
 * load average (I think this is common to all Unices) and we just normalize it.
 * Note that this is quite coarse, since we take into consideration only the VM
 * load.
 *
 * Load average is a tuple of 3 floating point values, representing the load
 * in the last 1, 5, 15 minutes.
 * The range is not clearly specified, can be greater than 1, but 1 already
 * means that the CPU is idle 0%, at least on a uniprocessor system.
 */
static apr_status_t
lnx_probe_calc_average(dfp_probe_ctx_t *ctx, int *value)
{
    double load_1min;
    int load_i;

    ctx = NULL;
    /* In case of error, we put the server off-line. This should ring a
     * bell to the system administrator.
     */
    *value = 0;

    errno = 0; /* not sure if getloadavg sets errno on failure */
    if (getloadavg(&load_1min, 1) < 0) {
        cpe_log(CPE_WARN, "failed to get loadavg: %s", strerror(errno));
        return APR_EGENERAL;
    }

    /* Linux (compared to fbsd) already gives use the value properly scaled,
     * so we just have to normalize according to our (DFP) convention: 0..100
     */
    load_i = load_1min * 100;
    if (load_i < 0) {
        cpe_log(CPE_WARN, "loadavg out of range %d", load_i);
        return APR_EGENERAL;
    }
    if (load_i > 100) {
        cpe_log(CPE_DEB, "loadavg very high (%d), rescaled to max", load_i);
        load_i = 100;
    }
    /* Finally map from load (0..100) to DFP weight (100..0) */
    load_i = 100 - load_i;
    *value = load_i;

    return APR_SUCCESS;
}
