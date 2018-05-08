import unittest
import itertools
import rpy2.rinterface as rinterface


try:
    import numpy
    has_numpy = True
except ImportError:
    has_numpy = False

rinterface.initr()

def floatEqual(x, y, epsilon = 0.00000001):
    return abs(x - y) < epsilon


class SexpVectorNumericTestCase(unittest.TestCase):

    numericmodule = numpy
    
    @unittest.skipUnless(has_numpy, 'Package numpy is not installed.')
    def testArrayStructInt(self):
        px = [1, -2, 3]
        x = rinterface.SexpVector(px, rinterface.INTSXP)
        nx = self.numericmodule.asarray(x)
        self.assertEqual(nx.dtype.kind, 'i')
        for orig, new in zip(px, nx):
            self.assertEqual(orig, new)

        # change value in the Python array... makes it change in the R vector
        nx[1] = 12
        self.assertEqual(x[1], 12)
        
    @unittest.skipUnless(has_numpy, 'Package numpy is not installed.')
    def testArrayStructDouble(self):
        px = [1.0, -2.0, 3.0]
        x = rinterface.SexpVector(px, rinterface.REALSXP)
        nx = self.numericmodule.asarray(x)
        self.assertEqual(nx.dtype.kind, 'f')
        for orig, new in zip(px, nx):
            self.assertEqual(orig, new)

        # change value in the Python array... makes it change in the R vector
        nx[1] = 333.2
        self.assertEqual(x[1], 333.2)
        
    @unittest.skipUnless(has_numpy, 'Package numpy is not installed.')
    def testArrayStructComplex(self):
        px = [1+2j, 2+5j, -1+0j]
        x = rinterface.SexpVector(px, rinterface.CPLXSXP)
        nx = self.numericmodule.asarray(x)
        self.assertEqual(nx.dtype.kind, 'c')
        for orig, new in zip(px, nx):
            self.assertEqual(orig, new)

    @unittest.skipUnless(has_numpy, 'Package numpy is not installed.')
    def testArrayStructBoolean(self):
        px = [True, False, True]
        x = rinterface.SexpVector(px, rinterface.LGLSXP)
        nx = self.numericmodule.asarray(x)
        self.assertEqual('i', nx.dtype.kind) # not 'b', see comments in array.c
        for orig, new in zip(px, nx):
            self.assertEqual(orig, new)

    @unittest.skipUnless(has_numpy, 'Package numpy is not installed.')
    def testArrayShapeLen3(self):
        extract = rinterface.baseenv['[']
        rarray = rinterface.baseenv['array'](rinterface.IntSexpVector(range(30)),
                                             dim = rinterface.IntSexpVector([5,2,3]))
        npyarray = numpy.array(rarray)
        for i in range(5):
            for j in range(2):
                for k in range(3):
                    self.assertEqual(extract(rarray, i+1, j+1, k+1)[0], 
                                      npyarray[i, j, k])

def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(SexpVectorNumericTestCase)
    return suite

if __name__ == '__main__':
    tr = unittest.TextTestRunner(verbosity = 2)
    tr.run(suite())

