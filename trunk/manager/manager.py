#!/usr/bin/env python
#
# Cisco Portable Events (CPE)
# Marco Molteni, October 2006.
# $Id: manager.py 129 2007-03-27 08:33:14Z mmolteni $

import asyncore, socket

class dfp_manager(asyncore.dispatcher):

    def __init__(self, host, port):
        asyncore.dispatcher.__init__(self)
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.connect( (host, port) )
        self.buffer = ''

    def handle_connect(self):
        pass

    def handle_close(self):
        self.close()

    def handle_read(self):
        print self.recv(8192)

    def writable(self):
        return (len(self.buffer) > 0)

    def handle_write(self):
        sent = self.send(self.buffer)
        self.buffer = self.buffer[sent:]





c = dfp_manager('localhost', 8081)

asyncore.loop()
