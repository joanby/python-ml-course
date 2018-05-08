import unittest
import pytz
import sys
import rpy2.robjects as robjects
from rpy2.robjects import conversion
import rpy2.rinterface as rinterface

from collections import OrderedDict
from datetime import datetime

has_pandas = True
try:
    import pandas
    import numpy
    has_pandas = True
except:
    has_pandas = False

if has_pandas:
    import rpy2.robjects.pandas2ri as rpyp

from rpy2.robjects import default_converter
from rpy2.robjects.conversion import Converter, localconverter
    
@unittest.skipUnless(has_pandas, "The Python package 'pandas' is not installed: functionalities associated with it cannot be tested.")
class PandasConversionsTestCase(unittest.TestCase):

    def testActivate(self):
        #FIXME: is the following still making sense ?
        self.assertNotEqual(rpyp.py2ri, robjects.conversion.py2ri)
        l = len(robjects.conversion.py2ri.registry)
        k = set(robjects.conversion.py2ri.registry.keys())
        rpyp.activate()
        self.assertTrue(len(conversion.py2ri.registry) > l)
        rpyp.deactivate()
        self.assertEqual(l, len(conversion.py2ri.registry))
        self.assertEqual(k, set(conversion.py2ri.registry.keys()))

    def testActivateTwice(self):
        #FIXME: is the following still making sense ?
        self.assertNotEqual(rpyp.py2ri, robjects.conversion.py2ri)
        l = len(robjects.conversion.py2ri.registry)
        k = set(robjects.conversion.py2ri.registry.keys())
        rpyp.activate()
        rpyp.deactivate()
        rpyp.activate()
        self.assertTrue(len(conversion.py2ri.registry) > l)
        rpyp.deactivate()
        self.assertEqual(l, len(conversion.py2ri.registry))
        self.assertEqual(k, set(conversion.py2ri.registry.keys()))

    def testDataFrame(self):
        # Content for test data frame
        l = (('b', numpy.array([True, False, True], dtype=numpy.bool_)),
             ('i', numpy.array([1, 2, 3], dtype="i")),
             ('f', numpy.array([1, 2, 3], dtype="f")),
             ('s', numpy.array(["b", "c", "d"], dtype="S")),
             ('u', numpy.array([u"a", u"b", u"c"], dtype="U")),
             ('dates', [datetime(2012, 5, 2), 
                        datetime(2012, 6, 3), 
                        datetime(2012, 7, 1)]))
        od = OrderedDict(l)
        # Pandas data frame
        pd_df = pandas.core.frame.DataFrame(od)
        # Convert to R
        with localconverter(default_converter + rpyp.converter) as cv:
            rp_df = robjects.conversion.py2ri(pd_df)
        self.assertEqual(pd_df.shape[0], rp_df.nrow)
        self.assertEqual(pd_df.shape[1], rp_df.ncol)
        if sys.version_info[0] == 3:
            self.assertSequenceEqual(rp_df.rx2('s'), [b"b", b"c", b"d"])
        else:
            # if Python it will be a string, that will turn into a factor
            col_s = rp_df.rx2('s')
            self.assertEqual(robjects.vectors.FactorVector,
                             type(col_s))
            self.assertSequenceEqual(col_s.levels, [b"b", b"c", b"d"])
            
    def testSeries(self):
        Series = pandas.core.series.Series
        s = Series(numpy.random.randn(5), index=['a', 'b', 'c', 'd', 'e'])
        with localconverter(default_converter + rpyp.converter) as cv:
            rp_s = robjects.conversion.py2ri(s)
        self.assertEqual(rinterface.FloatSexpVector, type(rp_s))

    def testSeries_issue264(self):
        Series = pandas.core.series.Series
        s = Series(('a', 'b', 'c', 'd', 'e'),
                   index=pandas.Int64Index([0,1,2,3,4]))
        with localconverter(default_converter + rpyp.converter) as cv:
            rp_s = robjects.conversion.py2ri(s)
        # segfault before the fix
        str(rp_s)
        self.assertEqual(rinterface.StrSexpVector, type(rp_s))

    def testObject2String(self):
        series = pandas.Series(["a","b","c","a"], dtype="O")
        with localconverter(default_converter + rpyp.converter) as cv:
            rp_c = robjects.conversion.py2ro(series)
            self.assertEqual(robjects.vectors.StrVector, type(rp_c))

    def testFactor2Category(self):
        factor = robjects.vectors.FactorVector(('a', 'b', 'a'))
        with localconverter(default_converter + rpyp.converter) as cv:
            rp_c = robjects.conversion.ri2py(factor)
        self.assertEqual(pandas.Categorical, type(rp_c))

    def testOrderedFactor2Category(self):
        factor = robjects.vectors.FactorVector(('a', 'b', 'a'), ordered=True)
        with localconverter(default_converter + rpyp.converter) as cv:
            rp_c = robjects.conversion.ri2py(factor)
        self.assertEqual(pandas.Categorical, type(rp_c))

    def testCategory2Factor(self):
        category = pandas.Series(["a","b","c","a"], dtype="category")
        with localconverter(default_converter + rpyp.converter) as cv:
            rp_c = robjects.conversion.py2ro(category)
            self.assertEqual(robjects.vectors.FactorVector, type(rp_c))
            
    def testOrderedCategory2Factor(self):
        category = pandas.Series(pandas.Categorical(['a','b','c','a'],
                                                    categories=['a','b','c'],
                                                    ordered=True))
        with localconverter(default_converter + rpyp.converter) as cv:
            rp_c = robjects.conversion.py2ro(category)
            self.assertEqual(robjects.vectors.FactorVector, type(rp_c))

    def testTimeR2Pandas(self):
        tzone = rpyp.get_timezone()
        dt = [datetime(1960, 5, 2),
              datetime(1970, 6, 3), 
              datetime(2012, 7, 1)]
        dt = [x.replace(tzinfo=tzone) for x in dt]
        # fix the time
        ts = [x.timestamp() for x in dt]
        # Create an R POSIXct vector.
        r_time = robjects.baseenv['as.POSIXct'](rinterface.FloatSexpVector(ts),
                                                origin=rinterface.StrSexpVector(('1970-01-01',)))

        # Convert R POSIXct vector to pandas-compatible vector
        with localconverter(default_converter + rpyp.converter) as cv:
            py_time = robjects.conversion.ri2py(r_time)

        # Check that the round trip did not introduce changes
        for expected, obtained in zip(dt, py_time):
            self.assertEqual(expected, obtained.to_pydatetime())
        
        
    def testRepr(self):
        # this should go to testVector, with other tests for repr()
        l = (('b', numpy.array([True, False, True], dtype=numpy.bool_)),
             ('i', numpy.array([1, 2, 3], dtype="i")),
             ('f', numpy.array([1, 2, 3], dtype="f")),
             ('s', numpy.array(["a", "b", "c"], dtype="S")),
             ('u', numpy.array([u"a", u"b", u"c"], dtype="U")))
        od = OrderedDict(l)
        pd_df = pandas.core.frame.DataFrame(od)
        with localconverter(default_converter + rpyp.converter) as cv:
            rp_df = robjects.conversion.py2ri(pd_df)
        s = repr(rp_df) # used to fail with a TypeError
        s = s.split('\n')
        if sys.version_info[0] == 3:
            repr_str = '[BoolVec..., IntVector, FloatVe..., Vector, StrVector]'
        else:
            repr_str = '[BoolVec..., IntVector, FloatVe..., FactorV..., StrVector]'
        self.assertEqual(repr_str, s[2].strip())

    def testRi2pandas(self):
        rdataf = robjects.r('data.frame(a=1:2, '
                            '           b=I(c("a", "b")), '
                            '           c=c("a", "b"))')
        with localconverter(default_converter + rpyp.converter) as cv:
            pandas_df = robjects.conversion.ri2py(rdataf)

        self.assertIsInstance(pandas_df, pandas.DataFrame)
        self.assertEquals(('a', 'b', 'c'), tuple(pandas_df.keys()))
        self.assertEquals(pandas_df['a'].dtype, numpy.dtype('int32'))
        self.assertEquals(pandas_df['b'].dtype, numpy.dtype('O'))
        self.assertTrue(isinstance(pandas_df['c'].dtype,
                                   pandas.api.types.CategoricalDtype))
    
    def testRi2pandas_issue207(self):
        d = robjects.DataFrame({'x': 1})
        with localconverter(default_converter + rpyp.converter) as cv:
            try:
                ok = True
                robjects.globalenv['d'] = d
            except ValueError:
                ok = False
            finally:
                if 'd' in robjects.globalenv:
                    del(robjects.globalenv['d'])
        self.assertTrue(ok)

def suite():
    if has_pandas:
        return unittest.TestLoader().loadTestsFromTestCase(PandasConversionsTestCase)
    else:
        return unittest.TestLoader().loadTestsFromTestCase(MissingPandasDummyTestCase)

if __name__ == '__main__':
    unittest.main(defaultTest='suite')

