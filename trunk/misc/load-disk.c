/* $Id: load-disk.c 136 2007-03-27 12:27:01Z mmolteni $
 * Increase load on the disk.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int main(void)
{
    char fname[] = "load-disk-tmp-XXXXXX";
    char *buf;
    int fd, bufsize;

    /* 1. open file, truncate to 0 if it exists
     * 2. unlink it, so that it will disappear on program termination
     * 3. write 100 MB to it, parametrable
     * 4. close file
     * 5. sync/fsync
     * 6. goto 1.
     */

    bufsize = 100 * 1000000;
    if ( (buf = malloc(bufsize)) == NULL) {
        perror("malloc");
        exit(1);
    }
    if (mktemp(fname) == NULL) {
        perror("mktemp");
        exit(1);
    }
    //printf("Using file %s\n", fname);

    while (1) {
        /* this open will fail if the file already exists */
        if ( (fd = open(fname, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR )) == -1) {
            perror("open");
            exit(1);
        }
        if (unlink(fname) == -1) {
            perror("unlink");
            exit(1);
        }
        if (write(fd, buf, bufsize) == -1) {
            perror("write");
            exit(1);
        }
        if (fsync(fd) == -1) {
            perror("fsync");
            exit(1);
        }
        if (close(fd) == -1) {
            perror("close");
            exit(1);
        }
    }
    return 0;
}
