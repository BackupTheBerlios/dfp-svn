/*
 * Portable implementation of the Dynamic Feedback Protocol (DFP)
 *
 * $Id$
 *
 * Dummy probe to be used as stand-in until the plugin mechanism is working.
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
#include <assert.h>


static struct dummy_probe_ctx_ {
    int du_foo;
} dummy_probe_ctx;
typedef struct dummy_probe_ctx_ dummy_probe_ctx_t;


static apr_status_t
dummy_probe_take_measure(void *context, int *value);


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
    *probe_name         = "dummy plugin";
    *poll_interval      = apr_time_from_sec(5);
    *probe_take_measure = dummy_probe_take_measure;
    *take_measure_ctx   = &dummy_probe_ctx;
    /* Overriding probe_calc_average is NOT normally needed, since DFP already
     * does the calculation for you. Use it only if you know what you are doing.
     */
    *probe_calc_average = NULL;

    return APR_SUCCESS;
}


/*****************************************************************************
 *                               IMPLEMENTATION                              *
 *****************************************************************************/


/* Called periodically by the DFP probe subsystem.
 *
 * Here a real probe would gather a snapshot of the quantity it is due to
 * measure (CPU load, free memory, I/O, application-specific, whatever) and
 * return it in \p value.
 *
 * NOTE The DFP specs don't specify the weight range; they specify only that
 * a weight of 0 means full load, i.e. that server is not available for any
 * more flows.
 *
 * For the time being we use the convention that our values have a range
 * from 0 to 100, where 0 means full load and 100 means 0 load.
 */
static apr_status_t
dummy_probe_take_measure(void *context, int *value)
{
    dummy_probe_ctx_t *ctx = context;

    ctx->du_foo += 7;
    ctx->du_foo %= 100;
    *value = ctx->du_foo;

    /* If the measure fails, you should return APR_EGENERAL */

    return APR_SUCCESS;
}



