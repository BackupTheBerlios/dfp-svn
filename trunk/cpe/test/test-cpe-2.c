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

struct fdesc_data {
    int what;
};
#define SERVER 1
#define CLIENT 2

/* NOTE This test is incomplete, it doesn't handle short writes.
 */
static apr_status_t
mysend(apr_socket_t *sock, char *fmt, ...)
{
    va_list      ap;
    static char  buf[40];
    apr_size_t   sent;
    apr_status_t rv;

    //snprintf(buf, sizeof buf, "%s_%d", msg, i);
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    sent = sizeof buf;
    rv = apr_socket_send(sock, buf, &sent);
    ok(rv == APR_SUCCESS && sent == sizeof buf,
        "mysend: apr_socket_send (ok: %d), sending/sent %d/%d",
        rv == APR_SUCCESS, sizeof buf, sent);
    return rv;
}

static apr_status_t
myrecv(apr_socket_t *sock, const char *fname)
{
    static char buf[40];
    apr_size_t   received;
    apr_status_t rv;

    received = sizeof buf;
    rv = apr_socket_recv(sock, buf, &received);
    buf[(sizeof buf) -1] = '\0'; // might be binary
    //diag("%s: recv: \"%s\"", fname, buf);

    // EOF: remote end has closed its connection.
    ok(rv == APR_SUCCESS || APR_STATUS_IS_EOF(rv),
        "%s: apr_socket_recv (%s) (received %d)", fname, cpe_errmsg(rv),
        received);
    return rv;
}

/* If client, called on the connected socket when reqevents is/are ready;
 * if server, called on the accepted socket when reqevents is/are ready.
 *
 * Names "client" and "server" are misleading here; a server is simply someone
 * who waits for a connection, while a client is somehone who initiates the
 * connection. Once the connection is established, client and server become
 * equivalent peers.
 */
static apr_status_t
client_or_server_cb(void *ctx, apr_pollfd_t *pfd, cpe_event *e)
{
    apr_status_t       rv;
    cpe_network_ctx   *nc;
    char              *name;
    struct fdesc_data *uc;

    cpe_log(CPE_DEB, "socket %p, pfd %p, reqevents %#x, rtnevents %#x",
        pfd->desc.s, pfd, pfd->reqevents, pfd->rtnevents);
    cpe_event_add(e);

    nc = (cpe_network_ctx *) ctx;
    uc = (struct fdesc_data *) nc->nc_user_data;
    if (uc->what == SERVER) {
        name = "server";
    } else if (uc->what == CLIENT) {
        name = "client";
    } else {
        return APR_EGENERAL;
    }
    if (pfd->rtnevents & APR_POLLOUT) {
        if (nc->nc_count > 3) {
            /* NOTE disabling POLLOUT as we are doing here when we don't
             * have anything to send is CRITICAL to avoid using the CPU 100%.
             */
            rv = cpe_pollset_update(pfd, pfd->reqevents & ~APR_POLLOUT);
            //diag("cpe_pollset_update: %s", cpe_errmsg(rv));
            assert(rv == APR_SUCCESS);
            assert(nc->nc_count < 5);
            return APR_SUCCESS;
        }
        mysend(pfd->desc.s, "%s_%d", name, nc->nc_count);
        nc->nc_count++;
    }
    if (pfd->rtnevents & APR_POLLIN) {
        rv = myrecv(pfd->desc.s, __FUNCTION__);
        if (rv != APR_SUCCESS) {
            // for example remote end terminated the connection. We better
            // close this socket or we go in infinite loop
            apr_socket_close(pfd->desc.s);
            // TODO we should also remove this socket from the event system!!!
        }
    }
    return APR_SUCCESS;
}

apr_status_t
test_init(conf_t *conf)
{
    apr_status_t rv;

    conf->co_debug = CPE_INFO;
    conf->co_listen_port = SERVER_PORT;
    conf->co_loop_duration = apr_time_from_sec(1);
    CHECK(apr_pool_create(&conf->co_pool, NULL));

    plan_tests(26);

    return APR_SUCCESS;
}

apr_status_t
test_run(conf_t *conf)
{
    cpe_network_ctx   server_ctx, client_ctx;
    struct fdesc_data client_data, server_data;
    apr_int16_t       s_flags, c_flags;
    apr_time_t        runtime;

    memset(&server_ctx, 0, sizeof server_ctx);
    memset(&server_data, 0, sizeof server_data);
    server_ctx.nc_user_data = &server_data;
    server_data.what = SERVER;

    memset(&client_ctx, 0, sizeof client_ctx);
    memset(&client_data, 0, sizeof client_data);
    client_ctx.nc_user_data = &client_data;
    client_data.what = CLIENT;

    s_flags = c_flags = APR_POLLIN | APR_POLLOUT;
    test_network_io1(conf, &runtime,
        client_or_server_cb, &server_ctx, s_flags, NULL, NULL,
        client_or_server_cb, &client_ctx, c_flags, NULL, NULL);

    // As is, this is a bit too coarse grained.
    ok(runtime > conf->co_loop_duration,
        "runtime > loop duration (delta %lld ms)",
        apr_time_as_msec(runtime - conf->co_loop_duration));

    return APR_SUCCESS;
}
