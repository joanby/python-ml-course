import unittest
import copy
import gc
from rpy2 import rinterface

rinterface.initr()

class SexpSymbolTestCase(unittest.TestCase):

    def testNew_invalid(self):
        x = 1
        self.assertRaises(ValueError, rinterface.SexpSymbol, x)

    def testNew_missing(self):
        self.assertRaises(TypeError, rinterface.SexpSymbol)

    def testNew_fromstring(self):
        symbol = rinterface.SexpSymbol("pi")
        evalsymbol = rinterface.baseenv['eval'](symbol)
        self.assertEqual(evalsymbol.rid, rinterface.baseenv['pi'].rid)

    def testNew_str(self):
        symbol = rinterface.SexpSymbol("pi")
        self.assertEqual("pi", str(symbol))
        
def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(SexpSymbolTestCase)
    return suite

if __name__ == '__main__':
    tr = unittest.TextTestRunner(verbosity = 2)
    tr.run(suite())
