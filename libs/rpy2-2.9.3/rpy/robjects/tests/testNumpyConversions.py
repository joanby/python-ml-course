import unittest
import sys
import rpy2.robjects as robjects
import rpy2.robjects.conversion as conversion
r = robjects.r

has_numpy = True
try:
    import numpy
    import rpy2.robjects.numpy2ri as rpyn
except:
    has_numpy = False



@unittest.skipUnless(has_numpy, 'numpy is not available in python')
class NumpyConversionsTestCase(unittest.TestCase):

    def setUp(self):
        #self._py2ri = robjects.conversion.py2ri
        #self._ri2py = robjects.conversion.ri2py
        rpyn.activate()

    def tearDown(self):
        rpyn.deactivate()


    def testActivate(self):
        rpyn.deactivate()
        #FIXME: is the following still making sense ?
        self.assertNotEqual(rpyn.py2ri, conversion.py2ri)
        l = len(conversion.py2ri.registry)
        k = set(conversion.py2ri.registry.keys())
        rpyn.activate()
        self.assertTrue(len(conversion.py2ri.registry) > l)
        rpyn.deactivate()
        self.assertEqual(l, len(conversion.py2ri.registry))
        self.assertEqual(k, set(conversion.py2ri.registry.keys()))

    def testActivateTwice(self):
        rpyn.deactivate()
        #FIXME: is the following still making sense ?
        self.assertNotEqual(rpyn.py2ri, conversion.py2ri)
        l = len(conversion.py2ri.registry)
        k = set(conversion.py2ri.registry.keys())
        rpyn.activate()
        rpyn.deactivate()
        rpyn.activate()
        self.assertTrue(len(conversion.py2ri.registry) > l)
        rpyn.deactivate()
        self.assertEqual(l, len(conversion.py2ri.registry))
        self.assertEqual(k, set(conversion.py2ri.registry.keys()))

    def checkHomogeneous(self, obj, mode, storage_mode):
        converted = conversion.py2ri(obj)
        self.assertEqual(r["mode"](converted)[0], mode)
        self.assertEqual(r["storage.mode"](converted)[0], storage_mode)
        self.assertEqual(list(obj), list(converted))
        self.assertTrue(r["is.array"](converted)[0])
        return converted

    def testVectorBoolean(self):
        l = [True, False, True]
        b = numpy.array(l, dtype=numpy.bool_)
        b_r = self.checkHomogeneous(b, "logical", "logical")
        self.assertTupleEqual(tuple(l), tuple(b_r))

    def testVectorInteger(self):
        l = [1, 2, 3]
        i = numpy.array(l, dtype="i")
        i_r = self.checkHomogeneous(i, "numeric", "integer")
        self.assertTupleEqual(tuple(l), tuple(i_r))
        
    def testVectorFloat(self):
        l = [1.0, 2.0, 3.0]
        f = numpy.array(l, dtype="f")
        f_r = self.checkHomogeneous(f, "numeric", "double")
        for orig, conv in zip(l, f_r):
            self.assertTrue(abs(orig-conv) < 0.000001)
        
    def testVectorComplex(self):
        l = [1j, 2j, 3j]
        c = numpy.array(l, dtype=numpy.complex_)
        c_r = self.checkHomogeneous(c, "complex", "complex")
        for orig, conv in zip(l, c_r):
            self.assertTrue(abs(orig.real-conv.real) < 0.000001)
            self.assertTrue(abs(orig.imag-conv.imag) < 0.000001)

    def testVectorUnicodeCharacter(self):
        l = [u"a", u"b", u"c"]
        u = numpy.array(l, dtype="U")
        u_r = self.checkHomogeneous(u, "character", "character")
        self.assertTupleEqual(tuple(l), tuple(u_r))

    def testArray(self):

        i2d = numpy.array([[1, 2, 3], [4, 5, 6]], dtype="i")
        i2d_r = conversion.py2ri(i2d)
        self.assertEqual(r["storage.mode"](i2d_r)[0], "integer")
        self.assertEqual(tuple(r["dim"](i2d_r)), (2, 3))

        # Make sure we got the row/column swap right:
        self.assertEqual(r["["](i2d_r, 1, 2)[0], i2d[0, 1])

        f3d = numpy.arange(24, dtype="f").reshape((2, 3, 4))
        f3d_r = conversion.py2ri(f3d)

        self.assertEqual(r["storage.mode"](f3d_r)[0], "double")
        self.assertEqual(tuple(r["dim"](f3d_r)), (2, 3, 4))

        # Make sure we got the row/column swap right:
        self.assertEqual(r["["](f3d_r, 1, 2, 3)[0], f3d[0, 1, 2])

    def testScalar(self):
        i32 = numpy.int32(100)
        i32_r = conversion.py2ri(i32)
        i32_test = numpy.array(i32_r)[0]
        self.assertEqual(i32, i32_test)

        i64 = numpy.int64(100)
        i64_r = conversion.py2ri(i64)
        i64_test = numpy.array(i64_r)[0]
        self.assertEqual(i64, i64_test)

    @unittest.skipUnless(has_numpy and hasattr(numpy, 'float128'),
                         "numpy.float128 not available on this system")
    def testScalar_f128(self):
        f128 = numpy.float128(100.000000003)
        f128_r = conversion.py2ri(f128)
        f128_test = numpy.array(f128_r)[0]
        self.assertEqual(f128, f128_test)

    def testObjectArray(self):
        o = numpy.array([1, "a", 3.2], dtype=numpy.object_)
        o_r = conversion.py2ri(o)
        self.assertEqual(r["mode"](o_r)[0], "list")
        self.assertEqual(r["[["](o_r, 1)[0], 1)
        self.assertEqual(r["[["](o_r, 2)[0], "a")
        self.assertEqual(r["[["](o_r, 3)[0], 3.2)

    def testRecordArray(self):
        rec = numpy.array([(1, 2.3), (2, -0.7), (3, 12.1)],
                          dtype=[("count", "i"), ("value", numpy.double)])
        rec_r = conversion.py2ri(rec)
        self.assertTrue(r["is.data.frame"](rec_r)[0])
        self.assertEqual(tuple(r["names"](rec_r)), ("count", "value"))
        count_r = r["$"](rec_r, "count")
        value_r = r["$"](rec_r, "value")
        self.assertEqual(r["storage.mode"](count_r)[0], "integer")
        self.assertEqual(r["storage.mode"](value_r)[0], "double")
        self.assertEqual(count_r[1], 2)
        self.assertEqual(value_r[2], 12.1)

    def testBadArray(self):
        u = numpy.array([1, 2, 3], dtype=numpy.uint32)
        self.assertRaises(ValueError, conversion.py2ri, u)

    def testAssignNumpyObject(self):
        x = numpy.arange(-10., 10., 1)
        env = robjects.Environment()
        env["x"] = x
        self.assertEqual(1, len(env))
        # do have an R object of the right type ?
        x_r = env["x"]
        self.assertEqual(robjects.rinterface.REALSXP, x_r.typeof)
        #
        self.assertEqual((20,), tuple(x_r.dim))


    def testDataFrameToNumpy(self):
        df = robjects.vectors.DataFrame(dict((('a', 1), ('b', 2))))
        rec = conversion.ri2py(df)
        self.assertEqual(numpy.recarray, type(rec))
        self.assertEqual(1, rec.a[0])
        self.assertEqual(2, rec.b[0])

    def testAtomicVectorToNumpy(self):
        v = robjects.vectors.IntVector((1,2,3))
        a = rpyn.ri2py(v)
        self.assertTrue(isinstance(a, numpy.ndarray))
        self.assertEqual(1, v[0])

    def testRx2(self):
        df = robjects.vectors.DataFrame({
            "A": robjects.vectors.IntVector([1,2,3]),
            "B": robjects.vectors.IntVector([1,2,3])})
        b = df.rx2('B')
        self.assertEquals(tuple((1,2,3)), tuple(b))

def suite():
    if has_numpy:
        return unittest.TestLoader().loadTestsFromTestCase(NumpyConversionsTestCase)
    else:
        return unittest.TestLoader().loadTestsFromTestCase(MissingNumpyDummyTestCase)

if __name__ == '__main__':
    unittest.main()

