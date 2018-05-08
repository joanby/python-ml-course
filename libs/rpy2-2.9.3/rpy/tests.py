#!/usr/bin/env python

'''tests.py - run all the tests worth running

The goal is that "ERRORS" and "FAILURES" are true failures, and expected
problems sould be dealt with using decorators.'''
from __future__ import print_function

# if `singledispatch` is absent, the unit tests are failing with a rather
# obscure / misleading error message. Test it now and report the problem. 
import sys
if sys.version_info[0] < 3 or (sys.version_info[0] == 3 and sys.version_info[1] < 4):
    try:
        from singledispatch import singledispatch
    except ImportError as ie:
        print('The module "singledispatch is required for Python < 3.4')
        raise ie

from os.path import dirname
import unittest

import rpy2
import rpy2.tests_rpy_classic

def load_tests(loader, tests, pattern):
    '''Run tests a little faster than with TestLoader.discover()

    Note that we are using the unittest API here, but blithely ignore the values
    passed in for `tests` and `pattern`'''
    # For some reason, the commented code directly below is slow and loads some
    # things twice One specific message is regarding package_dependencies from
    # the tools package.
    # rpy_root = dirname(rpy2.__file__)
    # alltests = unittest.defaultTestLoader.discover(rpy_root, pattern='test*')

    rpy_root = dirname(rpy2.__file__)
    tests = unittest.TestSuite()
    pattern = 'test*'

    # This now catches some extra tests (bypassing the suite() functions),
    # at least in a virtualenv that lacks various packages, like numpy &
    # pandas
    suite_robjects = loader.discover('robjects', pattern, rpy_root)
    suite_rinterface = loader.discover('rinterface', pattern, rpy_root)
    suite_rlike = loader.discover('rlike', pattern, rpy_root)
    # This is once again testless
    suite_interactive = loader.discover('interactive', pattern, rpy_root)
    # ipython is a little trickier because it is an optional
    # dependency.
    try:
        import IPython
        has_ipython = True
    except ImportError as ie:
        has_ipython = False

    if has_ipython:
        suite_ipython= loader.discover('ipython', pattern, rpy_root)
    else:
        class MissingIpythonTestCase(unittest.TestCase):
            @unittest.skip("The optional dependency IPython is required in order to test features using it.")
            def testHasIpython(self):
                pass
        suite_ipython = unittest.TestLoader().loadTestsFromTestCase(MissingIpythonTestCase)

    suite_rpy_classic = rpy2.tests_rpy_classic.suite()

    tests.addTests([suite_rinterface,
                    suite_robjects,
                    suite_rlike,
                    suite_interactive,
                    suite_ipython,
                    suite_rpy_classic
                    ])
    return tests


if __name__ == "__main__":
    import sys, rpy2.rinterface
    print("rpy2 version: %s" % rpy2.__version__)
    print("- built against R version: %s" % '-'.join(str(x)
          for x in rpy2.rinterface.R_VERSION_BUILD))

    try:
        import rpy2.rinterface
    except Exception as e:
        print("'rpy2.rinterface' could not be imported:")
        print(e)
        sys.exit(1)
    try:
        rpy2.rinterface.initr()
    except Exception as e:
        print("- The embedded R could not be initialized")
        print(e)
        sys.exit(1)
    try:
        rv = rpy2.rinterface.baseenv['R.version.string']
        print("- running linked to R version: %s" % rv[0])
    except KeyError as ke:
        print("The R version dynamically linked cannot be identified.")

    # This will sys.exit() with an appropriate error code
    unittest.main(buffer=True)
