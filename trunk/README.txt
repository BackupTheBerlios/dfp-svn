$Id: README.txt 137 2007-03-27 12:34:14Z mmolteni $

        Portable implementation of the Dynamic Feedback Protocol (DFP)
                        Cisco Portable Events (CPE)
                       Marco Molteni, Cisco Systems.


This code uses Apache APR as its portability layer, so should build and run
on at least POSIX systems (like the BSDs and Linux) and on Windows. It has been
built and tested on FreeBSD and Linux.

NOTE: This code is of BETA quality status. Although it has been tested
in a lab enviroment with load balancing hardware and showed quite stable,
it has NOT been tested in a production environment. You have been warned.

LICENSE

This code is being made available pursuant to the terms of the license
agreement detailed in the LICENSE file.

EXTERNAL PACKAGES

o Apache Portable Runtime (APR), the portability layer (http://apr.apache.org).
  NOTE: APR 1.2.7 has a bug in the FreeBSD implementation of apr_pollset_poll.
  If using FreeBSD, you need either the SVN trunk or a version >= 1.2.8.
o scons, a multi-platform substitute of make (http://www.scons.org). You
  need version >= 0.96.92.
o Python, a multi-platform scripting language, used for scons and for some of
  the test scripts (http://www.python.org). 
o libtap, a C implementation of the Perl Test Anything Protocol (TAP), a
  simple system for testing.
  (http://jc.ngo.org.uk/svnweb/jc/browse/nik/libtap/trunk)
  (In July 2006, the FreeBSD port of libtap is buggy, so download from svn
  instead).
  Note that libtap requires to be bootstrapped by the autotools; I try
  to avoid to require you to do this but it might fail.
  

BUILDING

Type "scons" in the dfp top-level directory.

TESTING

Before using dfp or cpe, be sure it passes all the tests: "scons test".

DOCUMENTATION

See directory doc in the dfp top-level directory.

CUSTOMIZING PROBES

See directory agent/plugins.

FEEDBACK AND BUG REPORTS

Although this code is not supported in any way by Cisco, I am interested in
making it completely portable and bug-free. Well-written bug reports with
reproductible problems and patches (preferably with regression tests) are
welcome, and will be looked at on a time-permitting basis.
