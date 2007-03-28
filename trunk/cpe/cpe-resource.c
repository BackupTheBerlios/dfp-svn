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

#include <stdlib.h>
#include <assert.h>

#include "apr_hash.h"

#include "cpe.h"
#include "cpe-logging.h"
#include "cpe-private.h"


typedef struct cpe_resource_node {
    struct cpe_resource_node *rn_next;
    cpe_event                *rn_event;
    /* used only in the head node; one pool per list */
    apr_pool_t               *rn_pool;
} cpe_resource_node_t;

/* singleton */
struct cpe_resource_table {
    apr_hash_t *hash_table;
    apr_pool_t *pool;       /* to create subpools */
} g_cpe_resource_blob;
struct cpe_resource_table *g_cpe_res = &g_cpe_resource_blob;


/*****************************************************************************
 *                                 INTERFACE                                 *
 *****************************************************************************/

/** Register the fact that event \p event is using socket \p sock, so that
 *  when destroying sock we can destroy all the registered events.
 *
 *  Rationale: from a data structure point of view, we want an hash
 *  table where a key can have multiple values: the key is the address
 *  of the socket and the value is a list of (addresses of) events.
 *  We cannot use apr_hash or apr_table directly because (at least in my
 *  understanding) they do not allow for duplicate values.
 */
apr_status_t
cpe_resource_register_user(apr_socket_t *sock, cpe_event *event)
{
    cpe_resource_node_t *new_res, *old_res;

    cpe_log(CPE_DEB, "%s", "enter");
    if (sock == NULL || event == NULL) {
        cpe_log(CPE_DEB, "invalid parms (sock %p, event %p)", sock, event);
        return APR_EINVAL;
    }

    new_res = apr_pcalloc(g_cpe_res->pool, sizeof(cpe_resource_node_t));
    new_res->rn_event = event;

    old_res = apr_hash_get(g_cpe_res->hash_table, sock, sizeof sock);
    if (old_res != NULL) {
        /* key "sock" already present; insert new_res in front */
        cpe_log(CPE_DEB, "sock %p found, head %p", sock, old_res);
        new_res->rn_next = old_res;
    } else {
        cpe_log(CPE_DEB, "sock %p not found", sock);
    }
    cpe_log(CPE_DEB, "inserting sock %p with head %p", sock, new_res);
    apr_hash_set(g_cpe_res->hash_table, sock, sizeof sock, new_res);

    return APR_SUCCESS;
}


/** Destroy all the events using this resource.
 */
apr_status_t
cpe_resource_destroy_users(apr_socket_t *sock)
{
    cpe_resource_node_t *head, *p;

    if (sock == NULL) {
        cpe_log(CPE_DEB, "invalid parms (sock %p)", sock);
        return APR_EINVAL;
    }
    head = apr_hash_get(g_cpe_res->hash_table, sock, sizeof sock);
    if (head == NULL) {
        cpe_log(CPE_DEB, "socket %p not present in the resource list", sock);
        return APR_SUCCESS;
    }
    /* XXX FIXME Here we are leaking node structs
     * rewrite to use a pool per list; this requires changing in 
     * cpe_resource_register_user the inserting of the new node; I must 
     * insert it at the end, so that the head cointains the pool to destroy
     */
    for (p = head; p != NULL; p = p->rn_next) {
        cpe_log(CPE_DEB, "socket %p: destroying event %p", sock, p->rn_event);
        cpe_event_destroy(&p->rn_event);
    }
    /* delete entry */
    apr_hash_set(g_cpe_res->hash_table, sock, sizeof sock, NULL);

    return APR_SUCCESS;
}


/*****************************************************************************
 *                               IMPLEMENTATION                              *
 *****************************************************************************/


/* Initialize the CPE Resource subsystem. Private use by the CPE framework.
 */
apr_status_t
cpe_resource_init(void)
{
    apr_status_t rv;

    CHECK(apr_pool_create(&g_cpe_res->pool, NULL));
    apr_pool_tag(g_cpe_res->pool, "cpe_resource");
    CHECK_NULL(g_cpe_res->hash_table, apr_hash_make(g_cpe_res->pool));
    return APR_SUCCESS;
}
