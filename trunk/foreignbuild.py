# $Id$

import os
import subprocess
import sys
import time

def run(cmd):
    print
    print 'executing %s in directory %s' % (cmd, os.getcwd())
    print
    try:
        retcode = -1
        retcode = subprocess.call(cmd)
        if retcode < 0:
            print >>sys.stderr, "Child was terminated by signal", -retcode
        else:
            print >>sys.stderr, "Child returned", retcode
    except OSError, e:
        print >>sys.stderr, "Execution failed:", e
    return retcode


def _configuremake2(base, reltarget, conf_args = [], bootstrap = False):
    print
    print 'Buiding foreign dependency using configure/make...'
    target = os.path.join(base, reltarget)
    target = os.path.join(os.getcwd(), target)
    if os.path.exists(target):
        print 'target %s exists, assuming already built' % target
        return 0

    os.chdir(base)
    if bootstrap:
        print 'bootstrapping...'
        ret = run(['sh', './bootstrap.sh'])
        if (ret != 0): return -1

    print
    print 'running configure...'
    cmd = ['./configure', '--prefix' , '%s' % target]
    cmd.extend(conf_args)
    ret = run(cmd)
    if (ret != 0): return -1

    print
    print 'running make...'
    ret = run(['make', 'install'])
    if (ret != 0): return -1
    ret = run(['make', 'clean'])
    return 0

def configuremake(base, reltarget, conf_args = [], bootstrap = False):
    cwd = os.getcwd()
    try:
        ret = _configuremake2(base, reltarget, conf_args, bootstrap)
    except:    
        os.chdir(cwd)
        raise
    os.chdir(cwd)
    return ret
