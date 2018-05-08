import unittest

import rpy2.rpy_classic as rpy
import rpy2.rinterface


class RpyClassicTestCase(unittest.TestCase):

    def testAttributeExpansion(self):
        rpy.set_default_mode(rpy.BASIC_CONVERSION)
        wtest = rpy.r.wilcox_test
        self.assertTrue(isinstance(wtest, rpy.Robj))

    def testFunctionCall(self):
        rpy.set_default_mode(rpy.BASIC_CONVERSION)
        # positional only
        three = rpy.r.sum(1,2)
        three = three[0] # is this what is happening w/ rpy, or the list is
        # ...automatically dropped ?
        self.assertEqual(3, three)
        # positional + keywords
        onetwothree = rpy.r.seq(1, 3, by=0.5)
        self.assertEqual([1.0, 1.5, 2.0, 2.5, 3.0], onetwothree)

    def testFunctionCallWithRObj(self):
        rpy.set_default_mode(rpy.NO_CONVERSION)
        onetwo = rpy.r.seq(1, 2)
        three = rpy.r.sum(onetwo)
        rpy.set_default_mode(rpy.BASIC_CONVERSION)
        self.assertEqual(3, three.sexp[0])

    def testCallable(self):
        rpy.set_default_mode(rpy.NO_CONVERSION)
        #in rpy-1.x, everything is callable
        self.assertTrue(callable(rpy.r.seq))
        self.assertTrue(callable(rpy.r.pi))

    def testSexp(self):
        rpy.set_default_mode(rpy.NO_CONVERSION)
        pi = rpy.r.pi
        self.assertTrue(isinstance(pi.sexp, rpy2.rinterface.Sexp))
        self.assertRaises(AttributeError, setattr, pi, 'sexp', None)

def suite():
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(RpyClassicTestCase)
    return suite

if __name__ == '__main__':
     unittest.main()
