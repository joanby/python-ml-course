import unittest
import sys
if sys.version_info[0] == 2:
    from itertools import izip as zip
import rpy2.rlike.indexing as rfi

class OrderTestCase(unittest.TestCase):

    def testOrder(self):
        seq  = (  2,   1,   5,   3,   4)
        expected = (1, 2, 3, 4, 5)
        res = rfi.order(seq)
        for va, vb in zip(expected, res):
            self.assertEqual(va, seq[vb])


def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(OrderTestCase)
    #suite.addTest(unittest.TestLoader().loadTestsFromTestCase(VectorizeTestCase))
    return suite

if __name__ == '__main__':
     unittest.main()
