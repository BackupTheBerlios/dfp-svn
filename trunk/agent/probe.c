/*
 * Portable implementation of the Dynamic Feedback Protocol (DFP)
 * $Id: probe.c 136 2007-03-27 12:27:01Z mmolteni $
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

#include "dfp.h"
#include "dfp-private.h"
#include "probe.h"
#include "cpe-logging.h"

#define DFP_SAMPLES 60
struct dfp_probe_ctx_ {
    unsigned int        dp_count;
    /* circular buffer, requires modulo N arithmetic */
    int                 dp_samples[DFP_SAMPLES];
    int                 dp_index;
    apr_time_t          dp_poll_interval;
    dfp_take_measure_t  dp_take_measure_cb;
    void               *dp_take_measure_ctx;
};


static apr_status_t
dfp_probe_cb(void *context, apr_pollfd_t *pfd, cpe_event *e);


/*****************************************************************************
 *                                 INTERFACE                                 *
 *****************************************************************************/


/** Find and initialize the probe callbacks.
 */
apr_status_t
dfp_probe_init(apr_pool_t *pool)
{
    apr_status_t        rv;
    apr_time_t          poll_interval;
    dfp_take_measure_t  take_measure_cb = NULL;
    void               *take_measure_ctx = NULL;
    cpe_event          *event;
    dfp_probe_ctx_t    *event_ctx;
    dfp_calc_average_t  calc_average_cb = NULL;
    char               *probe_name = NULL;

    /* For each found probe: create a timer event with dfp_probe_cb()
     * as callback;
     *
     * TODO: write the for each loop. For the time being we use only the
     * dummy probe.
     *
     * TODO: maybe create a subpool per probe?
     */

    /* Until we get a real plugin system, we just play a linker trick for
     * the probe_init() symbol.
     */
    g_dfp_probe_calc_average = dfp_probe_calc_average;
    CHECK(plugin_init(&probe_name, &poll_interval,
        &take_measure_cb, &take_measure_ctx, &calc_average_cb));
    cpe_log(CPE_INFO, "found probe: %s, poll interval %lld ms",
        probe_name, apr_time_as_msec(poll_interval));

    /* Given that the plugin is the same process as us, the maximum check we
     * can perform is wether the callback pointer is NULL or not.
     */
    if (take_measure_cb == NULL) {
        cpe_log(CPE_WARN, "%s", "warning: plugin init failed for xxx");
        /* Actually we should continue to the next plugin */
        return APR_EGENERAL;
    }
    if (calc_average_cb != NULL) {
        cpe_log(CPE_INFO, "%s", "plugin is overriding calc_average");
        g_dfp_probe_calc_average = calc_average_cb;
    }

    event_ctx = apr_pcalloc(pool, sizeof(dfp_probe_ctx_t));
    event_ctx->dp_poll_interval = poll_interval;
    event_ctx->dp_take_measure_cb = take_measure_cb;
    event_ctx->dp_take_measure_ctx = take_measure_ctx;

    CHECK_NULL(event,
        cpe_event_timer_create(poll_interval, dfp_probe_cb, event_ctx));

    /*XXX HACK */
    g_dfp_probe_ctx = event_ctx;

    return cpe_event_add(event);
}


/* Compute a N-moving average
 *
 * This is crude because the N and the polling interval which determines the
 * time width of the samples is hard-coded, but it is straightforward to make
 * it more flexible.
 *
 *
 */
apr_status_t
dfp_probe_calc_average(dfp_probe_ctx_t *ctx, apr_int32_t *value)
{
    unsigned int i, stop;
    int          avg;

    assert(ctx != NULL);

    avg = 0;
    /* "stop" is useful when the ring buffer is not full yet */
    stop = cpe_min(ctx->dp_count, DFP_SAMPLES);
    for (i = 0; i < stop; i++) {
        avg += ctx->dp_samples[i];
    }
    avg /= stop;
    *value = avg;

    return APR_SUCCESS;
}


/*****************************************************************************
 *                               IMPLEMENTATION                              *
 *****************************************************************************/


/* DESIGN POINT
 * Each probe plugin gets its own event, provided by DFP; this is to keep the
 * code of the plugin as small as possible. A plugin writer doesn't need to
 * know how to write a CPE event; it is enough for him to know a minimal API
 * just to:
 * 1. get registered at DFP startup
 * 2. tell DFP the measured value each time it is called
 */


static apr_status_t
dfp_probe_cb(void *context, apr_pollfd_t *pfd, cpe_event *e)
{
    apr_status_t     rv;
    int              value = -1;
    dfp_probe_ctx_t *ctx = context;

    cpe_log(CPE_DEB, "%s", "collecting data");
    assert(ctx != NULL);
    pfd = NULL;
    cpe_event_add(e);
    ctx->dp_count++;

    /* XXX Not sure it is enough to return on failure */
    CHECK(ctx->dp_take_measure_cb(ctx->dp_take_measure_ctx, &value));

    ctx->dp_samples[ctx->dp_index] = value;
    /* Increment modulo N */
    ctx->dp_index++;
    ctx->dp_index %= DFP_SAMPLES;

    return APR_SUCCESS;

}
