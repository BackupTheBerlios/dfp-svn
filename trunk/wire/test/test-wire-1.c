/*
 * Portable implementation of the Dynamic Feedback Protocol (DFP)
 *
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

#include <tap.h>
#include "wire.h"


int
main(void)
{
    plan_tests(9);

    /* Verify that the structs used for network packets have the correct
     * size.
     */

    ok1(sizeof(struct dfp_msg_header)          == 8);
    ok1(sizeof(struct dfp_tlv_header)          == 4);
    ok1(sizeof(struct dfp_tlv_security)        == 4+4);
    ok1(sizeof(struct dfp_tlv_security_md5)    == 20);
    ok1(sizeof(struct dfp_tlv_load_preference) == 8);
    ok1(sizeof(struct dfp_tlv_load)            == 4+8);
    ok1(sizeof(struct dfp_tlv_keepalive)       == 4+4);
    ok1(sizeof(struct dfp_tlv_bind_id)         == 12);
    ok1(sizeof(struct dfp_tlv_bind_id_table)   == 4+12);

    return exit_status();
}
