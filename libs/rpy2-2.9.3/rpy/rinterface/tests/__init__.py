import unittest
import sys

from . import test_SexpVector
from . import test_SexpEnvironment
from . import test_Sexp
from . import test_SexpSymbol
from . import test_SexpClosure
from . import test_SexpVectorNumeric
from . import test_Device
from . import test_SexpExtPtr

from . import test_EmbeddedR
#import test_EmbeddedR_multithreaded


def load_tests(loader, standard_tests, pattern):
    '''Ignore the test loader and return what we want

    Raw discovery here loads some stuff that results in a core dump, so
    we'll retain a load_tests() for now.'''
    suite_SexpVector = test_SexpVector.suite()
    suite_SexpEnvironment = test_SexpEnvironment.suite()
    suite_Sexp = test_Sexp.suite()
    suite_SexpSymbol = test_SexpSymbol.suite()
    suite_SexpClosure = test_SexpClosure.suite()
    suite_SexpVectorNumeric = test_SexpVectorNumeric.suite()
    suite_EmbeddedR = test_EmbeddedR.suite()
    suite_Device = test_Device.suite()
    suite_SexpExtPtr = test_SexpExtPtr.suite()
    #suite_EmbeddedR_multithreaded = test_EmbeddedR_multithreaded.suite()
    alltests = unittest.TestSuite([
        suite_EmbeddedR
        ,suite_Sexp
        ,suite_SexpSymbol
        ,suite_SexpVector 
        ,suite_SexpEnvironment 
        ,suite_SexpClosure
        ,suite_SexpVectorNumeric
        #,suite_Device
        #,suite_EmbeddedR_multithreaded
        ,suite_SexpExtPtr
        ])
    return alltests

def main():
    tr = unittest.TextTestRunner(verbosity = 2)
    # We implement the load_tests() API, but ignore what we get
    suite = load_tests(None, None, None)
    tr.run(suite)

if __name__ == '__main__':    
    main()
