#!/usr/bin/env python
import os, os.path, sys, shutil, re, itertools, warnings
import tempfile
import argparse, shlex, subprocess
from collections import namedtuple

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext as _build_ext


if sys.version_info[0] < 3:
    print("rpy2 is no longer supporting Python < 3. Consider using an older rpy2 release when using an older Python release.")
    sys.exit(1)

pack_name = 'rpy2'
pack_version = __import__('rpy').__version__

package_prefix='.'

R_MIN_VERSION = (3, 3)

def _format_version(x):
    return '.'.join(map(str, x))


def _download_r(url="https://cran.cnr.berkeley.edu/src/base/R-3/R-3.4.1.tar.gz"):
    """
    Highly experimental.

    Download the R source.
    """
    download_dir = tempfile.mkdtemp()
    r_src = os.path.join(download_dir, os.path.basename(url))
    cmd = ('wget', '-O', r_src, url)
    output = subprocess.check_output(cmd)
    return r_src

def _build_r(r_src):
    """
    Highly experimental.

    Build R from source.
    """
        
    assert r_src.endswith('tar.gz')
    cmd = ('tar', '-xzf', os.path.basename(r_src))
    r_src_dir = r_src[:-7]
    output = subprocess.check_output(cmd, cwd=os.path.dirname(r_src))

    cmd = ('./configure', '--enable-R-shlib')
    output = subprocess.check_output(cmd, cwd=r_src_dir)
    
    cmd = ('make',)
    output = subprocess.check_output(cmd, cwd=r_src_dir)
    
    cmd = ('make', 'check')
    output = subprocess.check_output(cmd, cwd=r_src_dir)
    return r_src_dir


def _install_r(r_src_dir, r_dest_dir):
    """
    Highly experimental.
    
    Run make install.
    """
    cmd = ('make', 'DESTDIR={:s}'.format(r_dest_dir), 'install')
    output = subprocess.check_output(cmd, cwd=r_src_dir)

    
def _get_r_home(r_bin = "R"):
    
    if (os.getenv('R_ENVIRON') is not None) or (os.getenv('R_ENVIRON_USER') is not None):
        warnings.warn("The environment variable R_ENVIRON or R_ENVIRON_USER is set. Differences between their settings during build time and run time may lead to issues when using rpy2.")

    try:
        r_home = subprocess.check_output((r_bin, "RHOME"),
                                         universal_newlines=True)
    except:
        msg = "Error: Tried to guess R's HOME but no command '%s' in the PATH." % r_bin
        print(msg)
        sys.exit(1)
        # try:
        #     r_src = _download_r()
        #     print(r_src)
        #     r_src_dir = _build_r(r_src)
        #     #_install_r(r_src_dir)
        # except:
        #     print("Unable to download R source.")
        #     sys.exit(1)

    r_home = r_home.split(os.linesep)

    #Twist if 'R RHOME' spits out a warning
    if r_home[0].startswith("WARNING"):
        warnings.warn("R emitting a warning: %s" % r_home[0])
        r_home = r_home[1].rstrip()
    else:
        r_home = r_home[0].rstrip()

    if os.path.exists(os.path.join(r_home, 'Renviron.site')):
        warnings.warn("The optional file '%s' is defined. Modifying it between build time and run time may lead to issues when using rpy2." % os.path.join(r_home, 'Renviron.site'))

    return r_home


class build_ext(_build_ext):
    """
    -DRPY_STRNDUP          : definition of strndup()
    -DRPY_VERBOSE
    -DRPY_DEBUG_PRESERV
    -DRPY_DEBUG_PROMISE    : evaluation of promises
    -DRPY_DEBUG_OBJECTINIT : initialization of PySexpObject
    -DRPY_DEBUG_CONSOLE    : console I/O
    -DRPY_DEBUG_COBJECT    : SexpObject passed as a CObject
    -DRPY_DEBUG_GRDEV
    """
    user_options = _build_ext.user_options + \
        [
            ('ignore-check-rversion', None, 'ignore checks for supported R versions')]

    boolean_options = _build_ext.boolean_options + \
        ['ignore-check-rversion', ] #+ \
        #['r-autoconfig', ]

    def initialize_options(self):
        try:
            super(build_ext, self).initialize_options()
        except TypeError:
            # distutils parent class an old style Python class
            _build_ext.initialize_options(self)
        self.r_autoconfig = None
        self.r_home = None
        self.r_home_lib = None
        self.ignore_check_rversion = False

    def finalize_options(self):
        self.set_undefined_options('build')
        r_home = _get_r_home()
        rexec = RExec(r_home)
        if rexec.version[0] == 'development':
            warnings.warn("Development version of R. Version compatibility check skipped.")
        elif cmp_version(rexec.version[:2], R_MIN_VERSION) == -1:
            if self.ignore_check_rversion:
                warnings.warn("R did not seem to have the minimum required version number")
            else:
                raise SystemExit("Error: R >= %s required"
                                 " (and the R we found is '%s')."
                                 % (_format_version(R_MIN_VERSION),
                                    _format_version(rexec.version)))

        try:
            super(build_ext, self).finalize_options() 
        except TypeError:
            # distutils parent class an old style Python class
            _build_ext.finalize_options(self)

    def run(self):
        try:
            super(build_ext, self).run()
        except TypeError:
            # distutils parent class an old style Python class
            _build_ext.run(self)

def cmp_version(x, y):
    if (x[0] < y[0]):
        return -1
    if (x[0] > y[0]):
        return 1
    if (x[0] == y[0]):
        if len(x) == 1 or len(y) == 1:
            return 0
        return cmp_version(x[1:], y[1:])

class RExec(object):
    """ Compilation-related configuration parameters used by R. """

    def __init__(self, r_home):
        if sys.platform == "win32" and "64 bit" in sys.version:
            r_exec = os.path.join(r_home, 'bin', 'x64', 'R')
        else:
            r_exec = os.path.join(r_home, 'bin', 'R')
        self._r_exec = r_exec
        self._version = None

    @property
    def version(self):
        if self._version is not None:
            return self._version
        output = subprocess.check_output((self._r_exec, '--version'), 
                                         universal_newlines = True)
        if not output:
            # sometimes R output goes to stderr
            output = subprocess.check_output((self._r_exec, '--version'), 
                                         stderr = subprocess.STDOUT,
                                         universal_newlines = True)
        output = iter(output.split('\n'))
        rversion = next(output)
        #Twist if 'R --version' spits out a warning
        if rversion.startswith("WARNING"):
            warnings.warn("R emitting a warning: %s" % rversion)
            rversion = next(output)
        print(rversion)
        m = re.match('^R ([^ ]+) ([^ ]+) .+$', rversion)
        if m is None:
            warnings.warn("Unable to extract R's version number from the string: '%s'" % rversion)
            # return dummy version 0.0
            rversion = [0, 0]
        else:
            rversion = m.groups()[1]
            if m.groups()[0] == 'version':
                rversion = [int(x) for x in rversion.split('.')]
            else:
                rversion = ['development', '']
        self._version = rversion
        return self._version

    def cmd_config(self, about, allow_empty=False):
        cmd = (self._r_exec, 'CMD', 'config', about)
        print(subprocess.list2cmdline(cmd))
        output = subprocess.check_output(cmd,
                                         universal_newlines = True)
        output = output.split(os.linesep)
        #Twist if 'R RHOME' spits out a warning
        if output[0].startswith("WARNING"):
            warnings.warn("R emitting a warning: %s" % output[0])
            output = output[1:]
        return output

def getRinterface_ext():
    extra_link_args = []
    extra_compile_args = []
    include_dirs = []
    libraries = []
    library_dirs = []

    #FIXME: crude way (will break in many cases)
    #check how to get how to have a configure step
    define_macros = []

    if sys.platform == 'win32':
        define_macros.append(('Win32', 1))
        if "64 bit" in sys.version:
            define_macros.append(('Win64', 1))
            extra_link_args.append('-m64')
            extra_compile_args.append('-m64')
            # MS_WIN64 only defined by pyconfig.h for MSVC. 
            # See http://bugs.python.org/issue4709
            define_macros.append(('MS_WIN64', 1))
    else:
        define_macros.append(('R_INTERFACE_PTRS', 1))
        define_macros.append(('HAVE_POSIX_SIGJMP', 1))
        define_macros.append(('RIF_HAS_RSIGHAND', 1))
        define_macros.append(('CSTACK_DEFNS', 1))
        define_macros.append(('HAS_READLINE', 1))


    if sys.byteorder == 'big':
        define_macros.append(('RPY_BIGENDIAN', 1))
    else:
        pass

    r_home = _get_r_home()
    rexec = RExec(r_home)
    if rexec.version[0] == 'development' or \
       cmp_version(rexec.version[:2], [3, 2]) == -1:
        warnings.warn("R did not seem to have the minimum required version number")

    ldf = shlex.split(' '.join(rexec.cmd_config('--ldflags')))
    cppf = shlex.split(' '.join(rexec.cmd_config('--cppflags')))
    #lapacklibs = rexec.cmd_config('LAPACK_LIBS', True)
    #blaslibs = rexec.cmd_config('BLAS_LIBS', True)

    parser = argparse.ArgumentParser()
    parser.add_argument('-I', action='append')
    parser.add_argument('-L', action='append')
    parser.add_argument('-l', action='append')

    # compile
    args, unknown = parser.parse_known_args(cppf)
    if args.I is None:
        warnings.warn('No include specified')
    else:
        include_dirs.extend(args.I)
    extra_compile_args.extend(unknown)
    # link
    args, unknown = parser.parse_known_args(ldf)
    # OS X's frameworks need special attention
    if args.L is None:
        # presumably OS X and framework:
        if args.l is None:
            # hmmm... no libraries at all
            warnings.warn('No libraries as -l arguments to the compiler.')
        else:
            libraries.extend([x for x in args.l if x != 'R'])
    else:
        library_dirs.extend(args.L)
        libraries.extend(args.l)
    extra_link_args.extend(unknown)
    
    print("""
    Compilation parameters for rpy2's C components:
        include_dirs    = %s
        library_dirs    = %s
        libraries       = %s
        extra_link_args = %s
    """ % (str(include_dirs),
           str(library_dirs), 
           str(libraries), 
           str(extra_link_args)))
    
    rinterface_ext = Extension(
            name = pack_name + '.rinterface._rinterface',
            sources = [os.path.join(package_prefix,
                                    'rpy', 'rinterface', '_rinterface.c')
                       ],
            depends = [os.path.join(package_prefix,
                                    'rpy', 'rinterface', 'embeddedr.h'), 
                       os.path.join(package_prefix,
                                    'rpy', 'rinterface', 'r_utils.h'),
                       os.path.join(package_prefix,
                                    'rpy', 'rinterface', 'buffer.h'),
                       os.path.join(package_prefix,
                                    'rpy', 'rinterface', 'sequence.h'),
                       os.path.join(package_prefix,
                                    'rpy', 'rinterface', 'sexp.h'),
                       os.path.join(package_prefix,
                                    'rpy', 'rinterface', '_rinterface.h'),
                       os.path.join(package_prefix,
                                    'rpy', 'rinterface', 'rpy_device.h')
                       ],
            include_dirs = [os.path.join(package_prefix,
                                         'rpy', 'rinterface'),] + include_dirs,
            libraries = libraries,
            library_dirs = library_dirs,
            define_macros = define_macros,
            runtime_library_dirs = library_dirs,
            extra_compile_args=extra_compile_args,
            extra_link_args = extra_link_args
            )

    rpy_device_ext = Extension(
        pack_name + '.rinterface._rpy_device',
        [
            os.path.join(package_prefix,
                         'rpy', 'rinterface', '_rpy_device.c'),
            ],
        include_dirs = include_dirs + 
        [os.path.join('rpy', 'rinterface'), ],
        libraries = libraries,
        library_dirs = library_dirs,
        define_macros = define_macros,
        runtime_library_dirs = library_dirs,
        extra_compile_args=extra_compile_args,
        extra_link_args = extra_link_args
        )

    return [rinterface_ext, rpy_device_ext]

LONG_DESCRIPTION = """
Python interface to the R language.

`rpy2` is running an embedded R, providing access to it from Python using R's own C-API through
either:
- a high-level interface making R functions an objects just like Python functions and providing
  a seamless conversion to numpy and pandas data strucutures
- a low-level interface closer to the C-API

It is also providing features for when working with jupyter notebooks or ipython.
"""

if __name__ == '__main__':
    rinterface_exts = []
    ri_ext = getRinterface_ext()
    rinterface_exts.extend(ri_ext)

    pack_dir = {pack_name: os.path.join(package_prefix, 'rpy')}

    #import setuptools.command.install
    #for scheme in setuptools.command.install.INSTALL_SCHEMES.values():
    #    scheme['data'] = scheme['purelib']

    if sys.version_info[0] == 2 and sys.version_info[1] < 7:
        print("rpy2 requires at least Python Version 2.7 (with Python 3.5 or later recommended).")
        sys.exit(1)
        
    requires=['six', 'jinja2']
    if sys.version_info[0] < 3 or (sys.version_info[0] == 3 and sys.version_info[1] < 4):
        requires.append('singledispatch')

    # additional C library included in rpy2
    libraries = list()
    libraries.append(('r_utils',
                      dict(sources = [os.path.join(package_prefix,
                                                   'rpy', 'rinterface',
                                                   'r_utils.c')],
                           include_dirs = ri_ext[0].include_dirs,
                           language = 'c')))
    
    setup(
        cmdclass = {'build_ext': build_ext},
        name = pack_name,
        version = pack_version,
        description = "Python interface to the R language (embedded R)",
        long_description = LONG_DESCRIPTION,
        url = "https://rpy2.bitbucket.io",
        license = "GPLv2+",
        author = "Laurent Gautier",
        author_email = "lgautier@gmail.com",
        requires = requires,
        install_requires = requires,
        ext_modules = rinterface_exts,
        libraries = libraries,
        package_dir = pack_dir,
        packages = [pack_name,
                    pack_name + '.rlike',
                    pack_name + '.rlike.tests',
                    pack_name + '.rinterface',
                    pack_name + '.rinterface.tests',
                    pack_name + '.robjects',
                    pack_name + '.robjects.tests',
                    pack_name + '.robjects.lib',
                    pack_name + '.robjects.lib.tests',
                    pack_name + '.interactive',
                    pack_name + '.interactive.tests',
                    pack_name + '.ipython',
                    pack_name + '.ipython.tests'
                    ],
        classifiers = ['Programming Language :: Python',
                       'Programming Language :: Python :: 3',
                       'Programming Language :: Python :: 3.5',
                       'License :: OSI Approved :: GNU General Public License v2 or later (GPLv2+)',
                       'Intended Audience :: Developers',
                       'Intended Audience :: Science/Research',
                       'Development Status :: 5 - Production/Stable'
                       ],
        package_data = {
            'rpy2': ['images/*.png', ],
            'rpy2': ['doc/source/rpy2_logo.png', ]}
        )

