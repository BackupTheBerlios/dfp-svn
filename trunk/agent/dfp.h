/*
 * Portable implementation of the Dynamic Feedback Protocol (DFP)
 *
 * $Id: dfp.h 136 2007-03-27 12:27:01Z mmolteni $
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

#ifndef DFP_INCLUDED
#define DFP_INCLUDED
/** @mainpage Cisco Dynamic Feedback Protocol (DFP)
 *
 *  This is a portable, open-source reference implementation of the Dynamic
 *  Feedback Protocol (DFP), as described in the IETF draft draft-eck-dfp-01.
 *  It contains two packages: DFP itself and CPE.
 *
 *  CPE is a C library to help write portable event-driven programs.
 *  Uses APR (Apache Portable Runtime) as its portability layer.
 *  Comes with a test suite.
 *
 *  Have a look at the "Modules" tab to get you started.
 *
 *  @par Licensing
 *  WRITEME
 */

/** @defgroup dfp DFP (Dynamic Feedback Protocol) core
 *
 *  This documentation describes both the DFP agent program and the DFP source
 *  code itself.
 *  @{
 */

#include "apr.h"
#include "apr_errno.h"
#include "apr_time.h"

#include "probe.h"

/** Callback used by DFP core to get the measured value from the plugin. */
typedef apr_status_t (*dfp_take_measure_t)(void *context, apr_int32_t *value);

/** Used by a plugin wanting to override calc_average. */
typedef apr_status_t (*dfp_calc_average_t)(dfp_probe_ctx_t *context,
    apr_int32_t *value);

/* Entry point in the DFP plugin system. This must be provided by the plugin.
 */
apr_status_t
plugin_init(
    char               **probe_name,
    apr_time_t          *poll_interval,
    dfp_take_measure_t  *probe_take_measure,
    void               **take_measure_ctx,
    dfp_calc_average_t  *probe_calc_average);


/* @} */
#endif /* DFP_INCLUDED */
