/*
 * Cisco Portable Events (CPE)
 * $Id: cpe.c 141 2007-03-28 08:19:35Z mmolteni $
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
#include "cpe-logging.h"
#include "cpe-private.h"

#include <stdlib.h>
#include <assert.h>

#define CPE_EV_MAGIC         0xcafefade

/**
 * @todo The best data structure to keep track of timers seems to be
 *       a priority queue. For the beginning, we focus on making the
 *       library work, so we use a non optimal ordered list implementation
 *       insert: O(N)
 *       find the maximum: O(1)
 *       Later, we will replace with a heap implementation
 *       insert: O(log2 N)
 *       find the maximum: O(1)
 *       or RB tree implementation
 *       insert: O(log2 N)
 *       find the maximum: O(log2 N)
 */

/* For APR internal use only. */
apr_pool_t            *g_cpe_pool;

static int             g_cpe_initialized;
static apr_time_t      g_cpe_start_time_us;
static apr_pollset_t  *g_cpe_pollset;
/* treat this as read-only, see cpe_pollset_add(), cpe_pollset_remove() */
static int             g_cpe_pollset_nelems;
static cpe_priorityQ  *g_cpe_eventQ;
static int             g_cpe_main_loop_done;


static void
cpe_assert_system_initialized(void)
{
    assert(g_cpe_initialized != 0);
}


static void
cpe_assert_event_ok(cpe_event *event)
{
    assert(event != NULL);
    assert(event->ev_magic == CPE_EV_MAGIC);
}


/*! Initialize the event system.
 * @param num_events  Number of events supported.
 * @see CPE_NUM_EVENTS_DEFAULT.
 * @todo we can make the library reentrant by passing all the resources to
 *       cpe_system_init (pool)    
 */
apr_status_t
cpe_system_init(apr_uint32_t num_events)
{
    apr_status_t rv;

    CHECK(rv = apr_initialize());
    atexit(apr_terminate);
    CHECK(rv = apr_pool_create(&g_cpe_pool, NULL));
    CHECK(rv = apr_pollset_create(&g_cpe_pollset, num_events, g_cpe_pool, 0));
    CHECK_NULL(g_cpe_eventQ, cpe_priorityQ_create());
    CHECK(rv = cpe_network_init(g_cpe_pool));
    CHECK(cpe_resource_init());

    g_cpe_start_time_us = apr_time_now();
    cpe_log(CPE_DEB, "start time: %lld ms",
        apr_time_as_msec(g_cpe_start_time_us));
    g_cpe_initialized = 1;
    return APR_SUCCESS;
}


static int
cpe_event_is_fdesc(cpe_event *event)
{
    apr_datatype_e type;

    cpe_assert_event_ok(event);
    type = event->ev_pollfd.desc_type;
    return type == APR_POLL_SOCKET || type == APR_POLL_FILE;
}


//static int
//cpe_event_is_timer(cpe_event *event)
//{
//    cpe_assert_event_ok(event);
//    return event->ev_pollfd.desc_type == APR_NO_DESC;
//}


static cpe_event *
cpe_event_create(
    cpe_ev_flags    flags,
    apr_datatype_e  desc_type, ///< pollfd
    apr_int16_t     reqevents, ///< pollfd
    apr_descriptor  desc,      ///< pollfd
    apr_time_t      timeout_us,
    cpe_callback_t  callback,
    void           *ctx)
{
    cpe_event *e;

    cpe_assert_system_initialized();

    if (desc_type == APR_NO_DESC) {
        /* requested a timer event */
        if (reqevents != 0 || desc.s != NULL) {
            cpe_log(CPE_ERR, "%s", "timer event but pollfd set");
            return NULL;
        }
        if (timeout_us <= 0) {
            cpe_log(CPE_ERR, "%s", "timer event but timeout <= 0");
            return NULL;
        }
    } else if (desc_type == APR_POLL_SOCKET || desc_type == APR_POLL_FILE) {
        /* requested a fdesc event */
        if (reqevents == 0 || desc.s == NULL) {
            cpe_log(CPE_ERR, "%s", "fdesc event but pollfd not set");
            return NULL;
        }
    } else {
        cpe_log(CPE_ERR, "%s", "invalid event type");
        return NULL;
    }
    /*!
     * \bug I don't use apr pool because I cannot release an apr_palloc.
     *      maybe I am missing something on the usage of pools, don't know.
     *      I could use a dedicated pool, created at cpe_init and destroyed
     *      at exit from cpe_main_loop.
     */
    e = calloc(1, sizeof *e);
    if (e == NULL) {
        cpe_log(CPE_ERR, "%s", "out of memory");
        return NULL;
    }

    e->ev_flags              = flags;

    e->ev_pollfd.desc_type   = desc_type;
    e->ev_pollfd.reqevents   = reqevents;
    e->ev_pollfd.desc        = desc;
    /* Backpointer to the event structure, used in the main event loop. */
    e->ev_pollfd.client_data = e;

    e->ev_timeout_us         = timeout_us;
    e->ev_callback           = callback;
    e->ev_ctx                = ctx;
    e->ev_magic              = CPE_EV_MAGIC;

    cpe_log(CPE_DEB, "created event %p", e);

    return e;
}


/*! Create a file/socket event, allocating memory and initializing its data
 *  structure.
 *
 * @param flags         event flags.
 * @param desc_type     See apr_pollfd_t
 * @param reqevents     See apr_pollfd_t
 * @param desc          See apr_pollfd_t
 * @param timeout_us    event timeout in us. If 0, wait indefinitely.
 * @param callback      callback function.
 * @param ctx           argument to the callback.
 * @return An handle to the created event, or NULL in case of failure.
 *
 * @see cpe_event_destroy().
 */
cpe_event *
cpe_event_fdesc_create(
    apr_datatype_e  desc_type, // pollfd
    apr_int16_t     reqevents, // pollfd
    apr_descriptor  desc,      // pollfd
    apr_time_t      timeout_us,
    cpe_callback_t  callback,
    void           *ctx)
{
    return cpe_event_create(0, desc_type, reqevents, desc, timeout_us,
        callback, ctx);
}


/*! Create a timer event, allocating memory and initializing its data
 *  structure.
 * @param flags         event flags.
 * @param timeout_us    event timeout in us. Cannot be 0.
 * @param callback      callback function.
 * @param ctx           argument to the callback.
 * @return An handle to the created event, or NULL in case of failure.
 */
cpe_event *
cpe_event_timer_create(
    apr_time_t      timeout_us,
    cpe_callback_t  callback,
    void           *ctx)
{
    apr_datatype_e desc_type = APR_NO_DESC;
    apr_int16_t    reqevents = 0;
    apr_descriptor desc      = {NULL};

    if (callback == NULL) {
        cpe_log(CPE_ERR, "%s", "timer event without callback (useless)");
        return NULL;
    }
    return cpe_event_create(0, desc_type, reqevents, desc, timeout_us,
        callback, ctx);
}


/* keep g_cpe_pollset_nelems in sync */
static apr_status_t
cpe_pollset_add(apr_pollset_t *pollset, apr_pollfd_t *pfd)
{
    apr_status_t rv;

    rv = apr_pollset_add(pollset, pfd);
    if (rv == APR_SUCCESS) {
        g_cpe_pollset_nelems++;
    }
    return rv;
}


/** keep g_cpe_pollset_nelems in sync
 *
 * @todo Looking at APR code, it is evident that different pollset
 * implementations behave differently. For example, in the select-based
 * implementation, the only parameter looked at in apr_pollset_remove is
 * the socket. On the other hand, in the kqueue-based implementation,
 * if reqevents don't match, apr_pollset_remove returns an error.
 *
 * This means that to safely remove a descriptor from the pollset, we need
 * to pass the same pollfd that has been used to perform apr_pollfd_add
 * (more exactly, a pollfd with the same socket (obviously) and the same
 * reqevents).
 */
static apr_status_t
cpe_pollset_remove(apr_pollset_t *pollset, apr_pollfd_t *pfd)
{
    apr_status_t rv;

    rv = apr_pollset_remove(pollset, pfd);
    if (rv == APR_SUCCESS) {
        g_cpe_pollset_nelems--;
    } else {
        cpe_log(CPE_ERR, "socket %p, apr_pollset_remove: %s",
            pfd->desc.s, cpe_errmsg(rv));
    }
    return rv;
}


/*! XXX Performance problem with APR (as I understand it):
 * to update the reqevents, we have to remove and then re-add
 * the pollfd to the pollset, and removal is O(N).
 *
 * @param pfd       The pfd on which to perform the pollset update. Note that
 *                  pfd itself will have it's reqevents updated too.
 * @param reqevents The new pfd poll flags.
 *
 * @remark This wrapper will go away as soon as APR provides an equivalent
 *         functionality.
 */
apr_status_t
cpe_pollset_update(apr_pollfd_t *pfd, apr_int16_t reqevents)
{
    apr_status_t rv;
    apr_pollfd_t old_pfd;

    assert(pfd != NULL);

    cpe_log(CPE_DEB, "updating socket %p from %#x to %#x",
        pfd->desc.s, pfd->reqevents, reqevents);
    if (pfd->reqevents == reqevents) {
        return APR_SUCCESS;
    }
    old_pfd = *pfd;
    pfd->reqevents = reqevents;
    rv = cpe_pollset_remove(g_cpe_pollset, &old_pfd);
    if (rv != APR_SUCCESS) {
        return rv;
    }
    return cpe_pollset_add(g_cpe_pollset, pfd);
}


/** Add an event to the event system, specifying the expiration time.
 * @see cpe_event_add()
 * @remarks If expiration is non-zero, the first expiration will be set
 *          at that value, but following expirations will be set using
 *          the timeout associated with the event.
 * @remarks If expiration is before current time, the event will trigger
 *          as soon as the event main loop is started.
 */
static apr_status_t
cpe_event_add3(
    cpe_event *event,      /**< event to add. */
    apr_time_t expiration, /**< absolute expiration time, in us. 0 makes this
                                function equivalent to cpe_event_add(). */
    int pollset_add)       /**< if non-zero, perform a pollset_add. */
{
    apr_status_t  rv;
    apr_time_t    time_now_us;
    cpe_priorityQ *q;

    cpe_assert_system_initialized();
    cpe_assert_event_ok(event);
    if (expiration < 0) {
        cpe_log(CPE_ERR, "negative expiration %lld us", expiration);
        return APR_EGENERAL;
    }

    /*! \todo check if the event is already in the queue, and fail
     *        if yes. mmhhh, this can become quite expensive...
     *        maybe just an hash table with the pointer address?
     */

    /*
     * Remember that in the CPE priority queue, priority means expiration time.
     */
    q = (cpe_priorityQ *) event;
    if (expiration != 0) {
        /* Explicit expiration, use it. */
        q->pq_value = expiration;
        cpe_log(CPE_DEB, "event %p, explicit_expiration %lld ms",
            event, apr_time_as_msec(expiration));
    } else {
        /* Implicit expiration, use event timeout. */
        time_now_us = apr_time_now();
        if (event->ev_timeout_us == 0) { /* block indefinitely */
            /** @bug
             *  XXX HACK WARNING using cpe_PRIORITYQ_AT_THE_END is too big at
             *  least for apr_pollset_poll/kqueue. Need to investigate more.
             *  For the time being we use a 24 max hours timeout (as kqueue).
             *  q->pq_value = cpe_PRIORITYQ_AT_THE_END;
             *
             *  BROKEN
             *  if we keep the 24 hour thing, we MUST be sure this max
             *  value is respected also for ->ev_timeout_us
             */
            q->pq_value = time_now_us + cpe_time_from_hour(24);
            cpe_log(CPE_DEB, "%s",
                "timeout 0 us -> block indefinitely (actually 24 hours)");
        } else {
            q->pq_value = time_now_us + event->ev_timeout_us;
        }
        cpe_log(CPE_DEB,
            "event %p, implicit_expiration %lld ms, ev_timeout %lld ms",
            event, apr_time_as_msec(q->pq_value),
            apr_time_as_msec(event->ev_timeout_us));
    }

    /* XXX Add to pollset only if not a timer event.
     * This needs to be changed if we go the way of having a dummy fdesc to be
     * able to keep also timer events in the pollset.
     */
    if (pollset_add && cpe_event_is_fdesc(event)) {
        rv = cpe_pollset_add(g_cpe_pollset, &event->ev_pollfd);
        if (rv != APR_SUCCESS) {
            cpe_log(CPE_ERR, "pollset_add: %s", cpe_errmsg(rv));
            return rv;
        }
    }
    cpe_priorityQ_insert(g_cpe_eventQ, q);

    return APR_SUCCESS;
}


/*! Add an event to the event system, specifying the expiration time.
 * @param event      the event to add.
 * @param expiration the absolute expiration time, in us. The value 0 makes
 *                   this function equivalent to cpe_event_add().
 *
 * @see cpe_event_add()
 * @remarks If expiration is non-zero, the first expiration will be set
 *          at that value, but following expirations will be set using
 *          the timeout associated with the event.
 * @remarks If expiration is before current time, the event will trigger
 *          as soon as the event main loop is started.
 */
apr_status_t
cpe_event_add2(cpe_event *event, apr_time_t expiration)
{
    return cpe_event_add3(event, expiration, 1);
}


/*! Add an event to the event system, setting its expiration based on the
 *  current time + the timeout specified in the event itself.
 *
 *  @param event The event to add.
 *  @see cpe_event_add2()
 */
apr_status_t
cpe_event_add(cpe_event *event)
{
    return cpe_event_add2(event, 0);
}


apr_status_t
cpe_event_set_timeout(cpe_event *event, apr_time_t timeout_us)
{
    cpe_assert_system_initialized();
    cpe_assert_event_ok(event);

    if (timeout_us <= 0) {
        cpe_log(CPE_ERR, "%s", "timeout <= 0");
        return APR_EINVAL;
    }
    cpe_log(CPE_DEB, "setting timeout on event %p to %lld ms", event,
        apr_time_as_msec(timeout_us));
    event->ev_timeout_us = timeout_us;
    return APR_SUCCESS;
}


int
cpe_events_in_system(void)
{
    cpe_assert_system_initialized();
    return cpe_priorityQ_len(g_cpe_eventQ);
}


static apr_status_t
cpe_system_queue_destroy(void)
{
    cpe_event    *e;
    cpe_priorityQ *q;

    cpe_assert_system_initialized();

    /*! @todo very bad performance wise, change with
     *        cpe_priorityQ_queue_destroy() with a callback to clean up the
     *        subclassed event
     */
    while ( (q = cpe_priorityQ_find_max(g_cpe_eventQ)) != NULL) {
        e = (cpe_event *) q;
        /* We assume that cpe_event_destroy() also removes from queue. */
        cpe_event_destroy(&e);
    }
    return APR_SUCCESS;
}


/*
 * If the pollset is empty, then apr_pollset_poll returns immediately
 * with "Invalid argument" (at least when the underlying implementation
 * is kqueue on fbsd). This sounds like a bug to me, since for example
 * select() can be used with empty fd_sets just as a timer.
 *
 * This forces us to test wether the pollset is empty or not
 * and take different actions.
 */
static apr_status_t
cpe_pollset_poll(apr_time_t timeout_us, apr_int32_t *num_pfd,
    const apr_pollfd_t **ret_pfd)
{
    apr_status_t rv;
    apr_time_t   start, stop;

    *num_pfd = 0;
    *ret_pfd = NULL;
    rv = APR_TIMEUP;
    if (timeout_us == 0 && g_cpe_pollset_nelems == 0) {
        cpe_log(CPE_DEB, "%s", "timeout 0 and pollset empty, skipping wait");
        return rv;
    }
    start = apr_time_now();
    cpe_log(CPE_DEB, "will_wait %lld ms, pollset_nelems %d",
        apr_time_as_msec(timeout_us), g_cpe_pollset_nelems);
    if (g_cpe_pollset_nelems == 0) {
        /* System contains only timer events. */
        if (timeout_us < 0) {
            cpe_log(CPE_ERR, "%s", "waiting forever on a timer event");
            return APR_EGENERAL;
        }
        apr_sleep(timeout_us);
    } else {
        int count = 0;
        do {
            rv = apr_pollset_poll(g_cpe_pollset, timeout_us, num_pfd, ret_pfd);
        } while (APR_STATUS_IS_EINTR(rv) && count++ < 5);
        if (rv != APR_SUCCESS && ! APR_STATUS_IS_TIMEUP(rv)) {
            cpe_log(CPE_ERR, "apr_pollset_poll: %s", cpe_errmsg(rv));
            return rv;
        }
    }
    stop = apr_time_now();
    cpe_log(CPE_DEB, "waited %lld ms, time_now %lld, desc_ready %d, rv %d",
        apr_time_as_msec(stop - start), apr_time_as_msec(stop), *num_pfd, rv);

    return rv;
}

#if 0
static apr_status_t
cpe_schedule_pollset_removal(cpe_event *event)
{
    event = NULL;

    return APR_SUCCESS;
}
#endif


/*! Remove the event from the event system and deallocate any resource
 *  used by it. On successful return, the handle becomes invalid.
 *
 * @todo rewrite using cpe_event_remove()
 */
apr_status_t
cpe_event_destroy(cpe_event **event)
{
    cpe_priorityQ *q;
    apr_status_t  rv = APR_SUCCESS;

    cpe_assert_system_initialized();
    cpe_assert_event_ok(*event);

    q = (cpe_priorityQ *) *event;
    /* Remove from priority queue. This will stop the timer. */
    if (cpe_priorityQ_remove(g_cpe_eventQ, q) != 1) {
        /* This is not an errror only if the event has not been added
         * to the system via cpe_event_add().
         */
        cpe_log(CPE_WARN, "event %p not in priority queue", *event);
    } else if (cpe_event_is_fdesc(*event)) {
        rv = cpe_pollset_remove(g_cpe_pollset, &(*event)->ev_pollfd);
    }
    free(*event);
    *event = NULL;
    return rv;
}


// @todo pollset removal should be done in lazy mode
apr_status_t
cpe_event_remove(cpe_event *event)
{
    cpe_priorityQ *q;
    apr_status_t  rv = APR_SUCCESS;

    cpe_assert_system_initialized();
    cpe_assert_event_ok(event);

    cpe_log(CPE_DEB, "removing event %p", event);
    q = (cpe_priorityQ *) event;
    /* Remove from priority queue. This will stop the timer. */
    if (cpe_priorityQ_remove(g_cpe_eventQ, q) != 1) {
        cpe_log(CPE_ERR, "event %p not in priority queue", event);
        return APR_EGENERAL;
    }
    if (cpe_event_is_fdesc(event)) {
        rv = cpe_pollset_remove(g_cpe_pollset, &event->ev_pollfd);
        //rv = cpe_schedule_pollset_removal(event);
    }
    return rv;
}


static cpe_event *
cpe_event_find_max(void)
{
    cpe_priorityQ *q;

    q = cpe_priorityQ_find_max(g_cpe_eventQ);
    return (cpe_event *) q;
}


// @todo pollset removal should be done in lazy mode
static apr_status_t
cpe_event_remove_max(cpe_event **max)
{
    cpe_priorityQ *q;
    apr_status_t  rv = APR_SUCCESS;

    q = cpe_priorityQ_remove_max(g_cpe_eventQ);
    *max = (cpe_event *) q;

    if (cpe_event_is_fdesc(*max)) {
        rv = cpe_pollset_remove(g_cpe_pollset, &(*max)->ev_pollfd);
        //rv = cpe_schedule_pollset_removal(event);
    }
    return rv;
}


static apr_status_t
cpe_event_commit_changes(void)
{
    // XXX WRITE ME!!!
#if 0
    if (e->ev_flags & CPE_EV_ONE_SHOT) {
        cpe_event_destroy(&e);
    } else {
        /** @bug XXX
         * this code doesn't take into consideration the case when, on
         * return from cpe_poll without a timeout, one of the events that
         * triggered (for I/O) is also _the_ event we removed from the
         * queue. The code will re-add the event with the same expiration,
         * effectively scheduling it one time too much.
         */
        if (APR_STATUS_IS_TIMEUP(rv)) {
            /* next expiration */
            cpe_event_add3(e, q->pq_value + e->ev_timeout_us, 0);
        } else {
            /* same expiration */
            cpe_event_add3(e, q->pq_value, 0);
        }
    }
#endif
    return APR_SUCCESS;
}


/** Schedule gracefully termination of main loop.
 */
void
cpe_main_loop_terminate(void)
{
    g_cpe_main_loop_done = 1;
}


static apr_status_t
cpe_master_timer_cb(void *ctx, apr_pollfd_t *pfd, cpe_event *e)
{
    ctx = NULL; pfd = NULL; e = NULL;
    return APR_SUCCESS;
}


/*!
 * Start the CPE event loop. On return, the event system is empty and all
 * the events are destroyed.
 *
 * @param max_wait_us Duration of the event loop. Use 0 for infinite duration.
 */
apr_status_t
cpe_main_loop(apr_time_t max_wait_us)
{
    u_int        loop_count = 1;
    apr_status_t rv = APR_EGENERAL;

    cpe_assert_system_initialized();

    if (max_wait_us < 0) {
        cpe_log(CPE_ERR, "negative max_wait %lld ms",
            apr_time_as_msec(max_wait_us));
        return APR_EINVAL;
    } else if (max_wait_us > 0) {
        cpe_event    *event;

        cpe_log(CPE_DEB, "max_wait %lld ms", apr_time_as_msec(max_wait_us));
        /* Internal event to keep track of the master timeout. */
        event = cpe_event_timer_create(max_wait_us, cpe_master_timer_cb,
            "master timer");
        if (event == NULL) {
            return APR_EGENERAL;
        }
        event->ev_flags |= CPE_EV_MASTER_TIMER;
        if (cpe_event_add(event) != APR_SUCCESS) {
            cpe_event_destroy(&event);
            return APR_EGENERAL;
        }
    } else {
        cpe_log(CPE_DEB, "%s", "max_wait 0, will loop forever");
    }

    while (1) {
        apr_time_t          timeout_us, time_now_us;
        cpe_priorityQ       *q_max;
        cpe_event          *e_max;
        apr_int32_t         num_pfd;
        const apr_pollfd_t *ret_pfd;

        time_now_us = apr_time_now();
        cpe_log(CPE_DEB, "enter_loop %5u (time_now %lld ms)",
            loop_count++, apr_time_as_msec(time_now_us));

        e_max = cpe_event_find_max();
        if (e_max == NULL) {
            cpe_log(CPE_DEB, "%s", "no more events, exiting main loop");
            break;
        }
        q_max = (cpe_priorityQ *) e_max;

        cpe_log(CPE_DEB, "event_max %p, expiration %lld ms",
            e_max, apr_time_as_msec(q_max->pq_value));

        timeout_us = q_max->pq_value - time_now_us;
        if (timeout_us < 0) {
            cpe_log(CPE_DEB, "delayed %lld ms",
                apr_time_as_msec(timeout_us));
            timeout_us = 0;
        }

        rv = cpe_pollset_poll(timeout_us, &num_pfd, &ret_pfd);
        if (APR_STATUS_IS_EINTR(rv)) {
            /* XXX not really sure of what we should do here... */
            cpe_log(CPE_DEB, "%s", "cpe_pollset_poll interrupted");
            continue;
        }
        if (rv != APR_SUCCESS && ! APR_STATUS_IS_TIMEUP(rv)) {
            break;
        }
        if (num_pfd > 0) {      /* Fdesc event(s). */
            int k;

            cpe_log(CPE_DEB, "%d fdesc events", num_pfd);
            /* scan the active files/sockets and invoke callbacks */
            for (k = 0; k < num_pfd; k++) {
                cpe_event *e2 = ret_pfd[k].client_data;

                cpe_assert_event_ok(e2);

                if (e2 == e_max && APR_STATUS_IS_TIMEUP(rv)) {
                    /* Assure callback will be invoked only once by skipping
                     * it now, will be called as a timed out event.
                     * NB: the sequencing is critical.
                     */
                    cpe_log(CPE_INFO, "%s",
                        "fdesc event is ready AND timed-out");
                    assert((e2->ev_flags & CPE_EV_MASTER_TIMER) == 0);
                    continue;
                }

                /* It is the callback responsability to re-add the event. */
                cpe_event_remove(e2);

                e2->ev_pollfd = ret_pfd[k];
                cpe_log(CPE_DEB, "Returned events %#x", e2->ev_pollfd.rtnevents);
                assert(e2->ev_callback != NULL);
                e2->ev_callback(e2->ev_ctx, &e2->ev_pollfd, e2);
            }
        }
        if (timeout_us == 0 || APR_STATUS_IS_TIMEUP(rv)) {     /* Timer event. */
            cpe_log(CPE_DEB, "%s", "timer event");
            /* It is the callback responsability to re-add the event. */
            rv = cpe_event_remove_max(&e_max);
            if (rv != APR_SUCCESS) {
                break;
            }
            if (e_max->ev_callback) {
                e_max->ev_callback(e_max->ev_ctx, &e_max->ev_pollfd, e_max);
            }
            if (e_max->ev_flags & CPE_EV_MASTER_TIMER) {
                cpe_log(CPE_DEB, "%s", "master timer elapsed");
                rv = APR_SUCCESS;
                break;
            }
        }
        rv = cpe_event_commit_changes();
        if (rv != APR_SUCCESS) {
            break;
        }
        if (g_cpe_main_loop_done) {
            cpe_log(CPE_DEB, "%s", "exiting from main loop as requested");
            break;
        }
    }
    /* Out of main loop. */
    if (rv != APR_SUCCESS) {
        return rv;
    }
    rv = cpe_system_queue_destroy();
    return rv;
}
