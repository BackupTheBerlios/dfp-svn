$Id$

        Portable implementation of the Dynamic Feedback Protocol (DFP)
                        Cisco Portable Events (CPE)
                       Marco Molteni, Cisco Systems.


This code uses Apache APR as its portability layer, so should build and run
on at least POSIX systems (like the BSDs and Linux) and on Windows. It has been
built and tested on FreeBSD and Linux.

NOTE: This code is BETA UNSUPPORTED software. Although it has been tested
in a lab enviroment with load balancing hardware and showed quite stable,
it has NOT been tested in a production environment. You have been warned.

LICENSE

This code is being made available pursuant to the terms of the license
agreement detailed in the LICENSE.txt file.

REQUIRED EXTERNAL PACKAGES

o Apache Portable Runtime (APR), the portability layer (http://apr.apache.org).
  NOTE: APR 1.2.7 has a bug in the FreeBSD implementation of apr_pollset_poll.
  If using FreeBSD, you need either the SVN trunk or a version >= 1.2.8.
o scons, a multi-platform substitute of make (http://www.scons.org). You
  need version >= 0.96.92.
o Python, a multi-platform scripting language, used for scons and for some of
  the test scripts (http://www.python.org). 
o The 'prove' utility from the Perl Test::Harness package (see next item)
o libtap, a C implementation of the Perl Test Anything Protocol (TAP), a
  simple system for testing.
  (http://jc.ngo.org.uk/svnweb/jc/browse/nik/libtap/trunk)
  You have two options:
  1. you can use the package libtap-snapshot that we provide as a convenience
     in the the dfp packages on berlios.de
  2. You can check it out as follows (in this case you have to bootstrap with
     autotools):
      cd dfp/trunk/external-libs
      svn co svn://jc.ngo.org.uk/nik/libtap/trunk libtap
  

BUILDING

Type "scons" in the dfp top-level directory.

TESTING

Before using dfp or cpe, be sure it passes all the tests: "scons test".

DOCUMENTATION

See directory doc in the dfp top-level directory.

METHOD TO CALCULATE THE LOAD / CUSTOMIZING PROBES

See directory agent/plugins.

What is provided right now is just a very crude load calculation, namely
the kernel load average in the last minute.

SECURITY

You don't need to be root to use the DFP agent, and from a security point of
view it is strongly advised that you create a special user just to run the
agent.

USAGE

./dfp-agent      -h will give you a list of the options.

By default dfp-agent listens only on localhost, so to make it actually
useful you must specify the IP address to listen on with the -a option:

./dfp-agent -a 10.0.0.1


FEEDBACK AND BUG REPORTS

Although this code is not supported in any way by Cisco, I am interested in
making it completely portable and bug-free. Well-written bug reports with
reproductible problems and patches (preferably with regression tests) are
welcome, and will be looked at on a time-permitting basis.
