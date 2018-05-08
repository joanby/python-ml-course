import unittest
import rpy2.robjects as robjects
rinterface = robjects.rinterface
import array


def almost_equal(x, y, epsilon = 0.00001):
    return abs(y - x) <= epsilon

class ArrayTestCase(unittest.TestCase):

    def testNew(self):
        letters = robjects.r.letters        
        self.assertRaises(TypeError, robjects.Array, letters)
        m = robjects.r.matrix(1, nrow=5, ncol=3)
        a = robjects.Array(m)
        # only tests that it runs.

    def testDim(self):
        m = robjects.r.matrix(1, nrow=5, ncol=3)
        a = robjects.Array(m)
        d = a.dim
        self.assertEqual(2, len(d))
        self.assertEqual(5, d[0])
        self.assertEqual(3, d[1])

#         rd = robjects.r.rev(d)
#         a.dim = rd


    def testNamesGet(self):
        dimnames = robjects.r.list(robjects.StrVector(['a', 'b', 'c']),
                                   robjects.StrVector(['d', 'e']))
        m = robjects.r.matrix(1, nrow=3, ncol=2,
                              dimnames = dimnames)
        a = robjects.Array(m)
        res = a.names
        r_identical = robjects.r.identical
        self.assertTrue(r_identical(dimnames[0], res[0]))
        self.assertTrue(r_identical(dimnames[1], res[1]))

    def testNamesSet(self):
        dimnames = robjects.r.list(robjects.StrVector(['a', 'b', 'c']),
                                   robjects.StrVector(['d', 'e']))
        m = robjects.r.matrix(1, nrow=3, ncol=2)
        a = robjects.Array(m)
        a.names = dimnames
        res = a.names
        r_identical = robjects.r.identical
        self.assertTrue(r_identical(dimnames[0], res[0]))
        self.assertTrue(r_identical(dimnames[1], res[1]))

class MatrixTestCase(unittest.TestCase):

    def testNrowGet(self):
        m = robjects.r.matrix(robjects.IntVector(range(6)), nrow=3, ncol=2)
        self.assertEqual(3, m.nrow)

    def testNcolGet(self):
        m = robjects.r.matrix(robjects.IntVector(range(6)), nrow=3, ncol=2)
        self.assertEqual(2, m.ncol)

    def testTranspose(self):
        m = robjects.r.matrix(robjects.IntVector(range(6)), nrow=3, ncol=2)
        mt = m.transpose()
        for i,val in enumerate((0,1,2,3,4,5,)):
            self.assertEqual(val, m[i])
        for i,val in enumerate((0,3,1,4,2,5)):
            self.assertEqual(val, mt[i])

    def testCrossprod(self):
        m = robjects.r.matrix(robjects.IntVector(range(4)), nrow=2)
        mcp = m.crossprod(m)
        for i,val in enumerate((1.0,3.0,3.0,13.0,)):
            self.assertEqual(val, mcp[i])

    def testTCrossprod(self):
        m = robjects.r.matrix(robjects.IntVector(range(4)), nrow=2)
        mtcp = m.tcrossprod(m)
        for i,val in enumerate((4,6,6,10,)):
            self.assertEqual(val, mtcp[i])

    def testSVD(self):
        m = robjects.r.matrix(robjects.IntVector((1, -1, -1, 1)), nrow=2)
        res = m.svd()
        for i,val in enumerate(res.rx2("d")):
            self.assertTrue(almost_equal((2, 0)[i], val))

    def testEigen(self):
        m = robjects.r.matrix(robjects.IntVector((1, -1, -1, 1)), nrow=2)
        res = m.eigen()
        for i, val in enumerate(res.rx2("values")):
            self.assertEqual((2, 0)[i], val)

    def testDot(self):
        m = robjects.r.matrix(robjects.IntVector(range(4)), nrow=2, ncol=2)
        m2 = m.dot(m)
        self.assertEqual((2,3,6,11), tuple(m2))

    def testColnames(self):
        m = robjects.r.matrix(robjects.IntVector(range(4)), nrow=2, ncol=2)
        self.assertEqual(rinterface.NULL, m.colnames)
        m.colnames = robjects.StrVector(('a', 'b'))
        self.assertEqual(2, len(m.colnames))
        self.assertEqual('a', m.colnames[0])
        self.assertEqual('b', m.colnames[1])
        self.assertRaises(ValueError, m.__setattr__, 'colnames', 
                          robjects.StrVector(('a', 'b', 'c')))

    def testRownames(self):
        m = robjects.r.matrix(robjects.IntVector(range(4)), nrow=2, ncol=2)
        self.assertEqual(rinterface.NULL, m.rownames)
        m.rownames = robjects.StrVector(('c', 'd'))
        self.assertEqual(2, len(m.rownames))
        self.assertEqual('c', m.rownames[0])
        self.assertEqual('d', m.rownames[1])
        self.assertRaises(ValueError, m.__setattr__, 'rownames', 
                          robjects.StrVector(('a', 'b', 'c')))

def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(ArrayTestCase)
    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(MatrixTestCase))
    return suite

if __name__ == '__main__':
     unittest.main()
