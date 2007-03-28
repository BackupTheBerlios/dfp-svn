/*
 * Portable implementation of the Dynamic Feedback Protocol (DFP)
 * $Id: test-getloadavg.c 136 2007-03-27 12:27:01Z mmolteni $
 *
 * Test Linux-specific probe.
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
#include <unistd.h>
#include <stdio.h>

#if 0
int
main()
{
    double loadavg[3];
    int n;

    while (1) {
        n = getloadavg(loadavg, 3);
        if (n != 3) {
            printf("getloadavg() returned %d\n", n);
        } else {
            printf("%f %f %f\n", loadavg[0], loadavg[1], loadavg[2]);
        }
        sleep(1);
    }
    return 0;
}
#endif

int
main()
{
    double loadavg;
    int n;

    while (1) {
        n = getloadavg(&loadavg, 1);
        if (n != 1) {
            printf("getloadavg() returned %d\n", n);
        } else {
            printf("%f\n", loadavg);
        }
        sleep(1);
    }
    return 0;
}
