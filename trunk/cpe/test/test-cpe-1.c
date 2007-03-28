/*
 * Cisco Portable Events (CPE)
 * $Id: test-cpe-1.c 136 2007-03-27 12:27:01Z mmolteni $
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


/* NOTE in this test, we are using only timer events. This means that the
 * CPE/APR code that will be tested for timeout is apr_sleep(), not
 * apr_pollset_poll().
 */
static void
test_timer_events(apr_time_t loop_duration)
{
    /* WARNING allocating the callback context on the stack, as we do here,
     * is safe only because we call cpe_main_loop() from this same function.
     */
    struct timer_data cbdata1 = {0, 0, 0, 0, 0, "cb1"};
    struct timer_data cbdata2 = {0, 0, 0, 0, 0, "cb2"};
    struct timer_data cbdata3 = {0, 0, 0, 0, 0, "cb3"};
    struct timer_data cbdata4 = {0, 0, 0, 0, 0, "cb4"};
    struct timer_data cbdata5 = {0, 0, 0, 0, 0, "cb5"};
    struct timer_data cbdata6 = {0, 0, 0, 0, 0, "cb6"};
    struct timer_data cbdata7 = {0, 0, 0, 0, 0, "cb7"};
    struct timer_data cbdata8 = {0, 0, 0, 0, 0, "cb8"};
    apr_time_t     time_now, runtime;
    cpe_event     *event;
    u_int          count;

    time_now = apr_time_now();

    cbdata1.last_seen = time_now;
    cbdata1.timeout = apr_time_from_sec(1);
    cbdata1.expected_count = 1;
    cbdata1.one_shot = 1;
    event = cpe_event_timer_create(cbdata1.timeout, test_timer_cb, &cbdata1);
    ok(event != NULL, "create event 1 (one-shot)");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event");

    CBDATA_INIT2(cbdata2, cpe_time_from_msec(200), time_now, loop_duration);
    event = cpe_event_timer_create(cbdata2.timeout, test_timer_cb, &cbdata2);
    ok(event != NULL, "create event 2");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event");

    CBDATA_INIT2(cbdata3, cpe_time_from_msec(200), time_now, loop_duration);
    event = cpe_event_timer_create(cbdata3.timeout, test_timer_cb, &cbdata3);
    ok(event != NULL, "create event 3");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event");

    CBDATA_INIT2(cbdata4, apr_time_from_sec(3), time_now, loop_duration);
    event = cpe_event_timer_create(cbdata4.timeout, test_timer_cb, &cbdata4);
    ok(event != NULL, "create event 4");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event");

    CBDATA_INIT2(cbdata5, apr_time_from_sec(1), time_now, loop_duration);
    event = cpe_event_timer_create(cbdata5.timeout, test_timer_cb, &cbdata5);
    ok(event != NULL, "create event 5");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event");

    CBDATA_INIT2(cbdata6, apr_time_from_sec(1), time_now, loop_duration);
    event = cpe_event_timer_create(cbdata6.timeout, test_timer_cb, &cbdata6);
    ok(event != NULL, "create event 6");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event");

    CBDATA_INIT2(cbdata7, cpe_time_from_msec(217), time_now, loop_duration);
    event = cpe_event_timer_create(cbdata7.timeout, test_timer_cb, &cbdata7);
    ok(event != NULL, "create event 7");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event");

    CBDATA_INIT2(cbdata8, cpe_time_from_msec(203), time_now, loop_duration);
    event = cpe_event_timer_create(cbdata8.timeout, test_timer_cb, &cbdata8);
    ok(event != NULL, "create event 8");
    ok(cpe_event_add(event) == APR_SUCCESS, "add event");

    loop_duration += cpe_time_from_msec(30);
    //diag("entering main loop for %d s", apr_time_sec(loop_duration));
    ok(cpe_main_loop(loop_duration) == APR_SUCCESS, "event main loop");

    count = cpe_events_in_system();
    ok(count == 0, "after main loop, event system empty (%d)", count);

    ok(cbdata1.count == 1, "cb1, one-shot timer (seen %d)", cbdata1.count);
    ok(cbdata2.expected_count == cbdata2.count, "cb2, expected %d, seen %d",
        cbdata2.expected_count, cbdata2.count);
    ok(cbdata3.expected_count == cbdata3.count, "cb3, expected %d, seen %d",
        cbdata3.expected_count, cbdata3.count);
    ok(cbdata4.expected_count == cbdata4.count, "cb4, expected %d, seen %d",
        cbdata4.expected_count, cbdata4.count);
    ok(cbdata5.expected_count == cbdata5.count, "cb5, expected %d, seen %d",
        cbdata5.expected_count, cbdata5.count);
    ok(cbdata6.expected_count == cbdata6.count, "cb6, expected %d, seen %d",
        cbdata6.expected_count, cbdata6.count);
    ok(cbdata7.expected_count == cbdata7.count, "cb7, expected %d, seen %d",
        cbdata7.expected_count, cbdata7.count);
    ok(cbdata8.expected_count == cbdata8.count, "cb8, expected %d, seen %d",
        cbdata8.expected_count, cbdata8.count);

    // TODO this should be a percentage
    runtime = apr_time_now() - time_now;
    ok(runtime > loop_duration, "runtime > loop duration (delta %lld ms)",
        apr_time_as_msec(runtime - loop_duration));

}

apr_status_t
test_init(conf_t *conf)
{
    conf->co_debug = CPE_INFO;
    conf->co_loop_duration = apr_time_from_sec(3);

    plan_tests(93);

    return APR_SUCCESS;
}

apr_status_t
test_run(conf_t *conf)
{
    test_timer_events(conf->co_loop_duration);
    return APR_SUCCESS;
}
