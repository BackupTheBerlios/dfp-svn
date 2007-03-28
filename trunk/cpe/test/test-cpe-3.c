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

#include "test-cpe-common.h"


/* NOTE in this test we use fdesc events, but without I/O, so to test the
 * timeout code of apr_pollset_poll().
 *
 * BUG: on Linux, I get I/O events (I don't understand why) and this breaks
 * the test. Need to investigate more.
 */
static void
test_fdesc_events2(conf_t *conf)
{
    /* WARNING allocating the callback context on the stack, as we do here,
     * is safe only because we call cpe_main_loop() from this same function.
     */
    struct timer_data cbdata1 = {0, 0, 0, 0, 0, "cb1"};
    struct timer_data cbdata2 = {0, 0, 0, 0, 0, "cb2"};
    struct timer_data cbdata3 = {0, 0, 0, 0, 0, "cb3"};
    struct timer_data cbdata4 = {0, 0, 0, 0, 0, "cb4"};
    struct timer_data cbdata5 = {0, 0, 0, 0, 0, "cb5"};
    apr_time_t     time_now, runtime;
    cpe_event     *event;
    apr_socket_t   *sock;
    apr_sockaddr_t *sockaddr;
    char           *hostname = "127.0.0.1";
    apr_port_t      port;
    int             count;

    time_now = apr_time_now();

    port = 5001;
    ok(cpe_socket_client_create(&sock, &sockaddr, hostname, port,
        conf->co_pool) == APR_SUCCESS, "cpe_socket_client_create");
    CBDATA_INIT2(cbdata1, cpe_time_from_msec(500), time_now,
        conf->co_loop_duration);
    event = cpe_event_fdesc_create(APR_POLL_SOCKET, APR_POLLIN,
        (apr_descriptor) sock, cbdata1.timeout, test_timer_cb, &cbdata1);
    ok(event != NULL, "create event 1");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event 1");

    port = 5002;
    ok(cpe_socket_client_create(&sock, &sockaddr, hostname, port,
        conf->co_pool) == APR_SUCCESS, "cpe_socket_client_create");
    CBDATA_INIT2(cbdata2, apr_time_from_sec(1), time_now,
        conf->co_loop_duration);
    event = cpe_event_fdesc_create(APR_POLL_SOCKET, APR_POLLIN,
        (apr_descriptor) sock, cbdata2.timeout, test_timer_cb, &cbdata2);
    ok(event != NULL, "create event 2");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event 2");

    port = 5003;
    ok(cpe_socket_client_create(&sock, &sockaddr, hostname, port,
        conf->co_pool) == APR_SUCCESS, "cpe_socket_client_create");
    CBDATA_INIT2(cbdata3, apr_time_from_sec(3), time_now,
        conf->co_loop_duration);
    event = cpe_event_fdesc_create(APR_POLL_SOCKET, APR_POLLIN,
        (apr_descriptor) sock, cbdata3.timeout, test_timer_cb, &cbdata3);
    ok(event != NULL, "create event 3");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event 3");

    port = 5004;
    ok(cpe_socket_client_create(&sock, &sockaddr, hostname, port,
        conf->co_pool) == APR_SUCCESS, "cpe_socket_client_create");
    CBDATA_INIT2(cbdata4, cpe_time_from_msec(100), time_now,
        conf->co_loop_duration);
    event = cpe_event_fdesc_create(APR_POLL_SOCKET, APR_POLLIN,
        (apr_descriptor) sock, cbdata4.timeout, test_timer_cb, &cbdata4);
    ok(event != NULL, "create event 4");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event 4");

    port = 5005;
    ok(cpe_socket_client_create(&sock, &sockaddr, hostname, port,
        conf->co_pool) == APR_SUCCESS, "cpe_socket_client_create");
    CBDATA_INIT2(cbdata5, cpe_time_from_msec(90), time_now,
        conf->co_loop_duration);
    event = cpe_event_fdesc_create(APR_POLL_SOCKET, APR_POLLIN,
        (apr_descriptor) sock, cbdata5.timeout, test_timer_cb, &cbdata5);
    ok(event != NULL, "create event 5");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event 5");


    //diag("entering main loop for %d s", apr_time_sec(loop_duration));
    conf->co_loop_duration += cpe_time_from_msec(50);
    ok(cpe_main_loop(conf->co_loop_duration) == APR_SUCCESS, "event main loop");
    runtime = apr_time_now() - time_now;

    count = cpe_events_in_system();
    ok(count == 0, "after main loop, event system empty (%d)", count);

    ok(cbdata1.expected_count == cbdata1.count, "cb1, expected %d, seen %d",
        cbdata1.expected_count, cbdata1.count);
    ok(cbdata2.expected_count == cbdata2.count, "cb2, expected %d, seen %d",
        cbdata2.expected_count, cbdata2.count);
    ok(cbdata3.expected_count == cbdata3.count, "cb3, expected %d, seen %d",
        cbdata3.expected_count, cbdata3.count);
    ok(cbdata4.expected_count == cbdata4.count, "cb4, expected %d, seen %d",
        cbdata4.expected_count, cbdata4.count);
    ok(cbdata5.expected_count == cbdata5.count, "cb5, expected %d, seen %d",
        cbdata5.expected_count, cbdata5.count);

    // TODO this should be a percentage
    ok(runtime > conf->co_loop_duration,
        "runtime > loop duration (delta %lld ms)",
        apr_time_as_msec(runtime - conf->co_loop_duration));

}

apr_status_t
test_init(conf_t *conf)
{
    apr_status_t rv;

    conf->co_debug = CPE_INFO;
    conf->co_listen_port = SERVER_PORT;
    conf->co_loop_duration = apr_time_from_sec(3);
    CHECK(apr_pool_create(&conf->co_pool, NULL));

    plan_tests(97);

    return APR_SUCCESS;
}

apr_status_t
test_run(conf_t *conf)
{
    test_fdesc_events2(conf);
    return APR_SUCCESS;
}
