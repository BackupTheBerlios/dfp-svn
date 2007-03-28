/*
 * Cisco Portable Events (CPE)
 * $Id: cpe.h 136 2007-03-27 12:27:01Z mmolteni $
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

#ifndef CPE_INCLUDED
#define CPE_INCLUDED
/** @defgroup cpe CPE (Cisco Portable Events) core
 *
 *  @par Events
 *  An event is an opaque handle returned by cpe_event_timer_create() 
 *  (timer event) or cpe_event_fdesc_create() (file or socket event, with
 *  optional timeout). Each event has an associated callback and its context.
 *  An event is one-shot; it is the callback responsability to re-add the
 *  event to the system with cpe_event_add().
 *
 *  @par CPE Thread model
 *  CPE is an event system, and assumes only one thread. Do not use
 *  any APR thread or synchronization primitives, otherwise CPE behavior
 *  is undefined. If you think this is a bug, let me say it again: CPE
 *  is an event system.
 *
 *  @par Callbacks must be non blocking
 *  Avoid any blocking call in a callback function associated with an
 *  event, otherwise the whole event system will block.  Think of a
 *  callback as a run-to-completion stackless thread.
 *
 *  @par Return values
 *  The functions try to follow the APR conventions of returning APR_SUCCESS
 *  for successful termination.
 *
 *  @par Debugging
 *  Extensive logging can be obtained by passing CPE_LOG_DEB to cpe_log_init().
 *
 *  @par Typical usage sequence for a timer event
 *  cpe_system_init()        <br>
 *  cpe_event_timer_create() <br>
 *  cpe_event_add()          <br>
 *  cpe_main_loop()
 *
 *  @par Typical usage sequence for a fdesc event
 *  cpe_system_init()        <br>
 *  cpe_event_fdesc_create() <br>
 *  cpe_event_add()          <br>
 *  cpe_main_loop()
 *
 *  @par Utility functions for a listening socket
 *  cpe_system_init()          <br>
 *  cpe_socket_server_create() <br>
 *  cpe_socket_after_accept()  <br>
 *  cpe_main_loop()
 *
 *  @par Utility functions for a connecting socket
 *  cpe_system_init()          <br>
 *  cpe_socket_client_create() <br>
 *  cpe_socket_after_connect() <br>
 *  cpe_main_loop()
 *
 *  @{
 */

#include <apr_time.h>
#include <apr_poll.h>
#include "cpe-algorithms.h"


/** Default number of events (descriptors + timers) supported. */
#define CPE_NUM_EVENTS_DEFAULT 128

/** Useful shorthands. */
#define CHECK(x) CHECK2(CPE_ERR, x)

#define CHECK2(log, x) do {                             \
    if ((rv = x) != APR_SUCCESS) {                      \
        cpe_log(log, "%s: %s", #x, cpe_errmsg(rv));     \
        return rv;                                      \
    }                                                   \
} while (0)

#define CHECK_NULL(y, x) do {                         \
    if ((y = x) == NULL) {                            \
        cpe_log(CPE_ERR, "%s: returned NULL", #x);    \
        return APR_EGENERAL;                          \
    }                                                 \
} while (0)


#define cpe_min(a,b) ((a) < (b) ? (a) : (b))

#define ONE_SI_KILO 1000
#define ONE_SI_MEGA 1000000


enum cpe_ev_flags {
    /* below only CPE internal events */
    CPE_EV_MASTER_TIMER = 0x01000000,
};
typedef enum cpe_ev_flags cpe_ev_flags;

typedef struct cpe_event cpe_event; /***< Opaque event handle. */
typedef apr_status_t (* cpe_callback_t)(void *ctx, apr_pollfd_t *pfd,
    cpe_event *e);

apr_status_t  cpe_system_init(apr_uint32_t pollset_size);
cpe_event    *cpe_event_fdesc_create(apr_datatype_e desc_type,
                apr_int16_t reqevents, apr_descriptor desc,
                apr_time_t timeout_us, cpe_callback_t callback, void *ctx);
cpe_event    *cpe_event_timer_create(apr_time_t timeout_us,
                cpe_callback_t callback, void *ctx);
apr_status_t  cpe_event_destroy(cpe_event **event);
apr_status_t  cpe_event_add(cpe_event *event);
apr_status_t  cpe_event_add2(cpe_event *event, apr_time_t expiration);
apr_status_t  cpe_event_set_timeout(cpe_event *event, apr_time_t timeout_us);
int           cpe_events_in_system(void);
apr_status_t  cpe_event_remove(cpe_event *event);
apr_status_t  cpe_pollset_update(apr_pollfd_t *pfd, apr_int16_t reqevents);
apr_status_t  cpe_main_loop(apr_time_t timeout_us);
void          cpe_main_loop_terminate(void);

/* from cpe-utils.c */
const char   *cpe_errmsg(apr_status_t rv);
apr_time_t    cpe_time_from_msec(int msec);
apr_time_t    cpe_time_from_hour(int hour);

/* from cpe-resource.c */
apr_status_t
cpe_resource_register_user(apr_socket_t *sock, cpe_event *event);
apr_status_t
cpe_resource_destroy_users(apr_socket_t *sock);



/* @} */
#endif /* CPE_INCLUDED */
