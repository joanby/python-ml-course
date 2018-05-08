import unittest
import rpy2.robjects as robjects
ri = robjects.rinterface
import array, time, sys
import time
import datetime
import rpy2.rlike.container as rlc
from collections import OrderedDict

if sys.version_info[0] == 2:
    range = xrange


rlist = robjects.baseenv["list"]

class VectorTestCase(unittest.TestCase):

    def testNew(self):
        identical = ri.baseenv["identical"]
        py_a = array.array('i', [1,2,3])
        ro_v = robjects.Vector(py_a)
        self.assertEqual(ro_v.typeof, ri.INTSXP)
        
        ri_v = ri.SexpVector(py_a, ri.INTSXP)
        ro_v = robjects.Vector(ri_v)

        self.assertTrue(identical(ro_v, ri_v)[0])

        del(ri_v)
        self.assertEqual(ri.INTSXP, ro_v.typeof)

    def testNewStrVector(self):
        vec = robjects.StrVector(['abc', 'def'])
        self.assertEqual('abc', vec[0])
        self.assertEqual('def', vec[1])
        self.assertEqual(2, len(vec))

    def testNewIntVector(self):
        vec = robjects.IntVector([123, 456])
        self.assertEqual(123, vec[0])
        self.assertEqual(456, vec[1])
        self.assertEqual(2, len(vec))

    def testNewFloatVector(self):
        vec = robjects.FloatVector([123.0, 456.0])
        self.assertEqual(123.0, vec[0])
        self.assertEqual(456.0, vec[1])
        self.assertEqual(2, len(vec))

    def testNewBoolVector(self):
        vec = robjects.BoolVector([True, False])
        self.assertEqual(True, vec[0])
        self.assertEqual(False, vec[1])
        self.assertEqual(2, len(vec))

    def _testNewListVector(self, vec):
        self.assertTrue('a' in vec.names)
        self.assertTrue('b' in vec.names)
        self.assertEqual(2, len(vec))
        self.assertEqual(2, len(vec.names))

    def testNewListVector(self):
        vec = robjects.ListVector({'a': 1, 'b': 2})
        self._testNewListVector(vec)
        s = (('a', 1), ('b', 2))
        vec = robjects.ListVector(s)
        self._testNewListVector(vec)
        it = iter(s)
        vec = robjects.ListVector(s)
        self._testNewListVector(vec)
        
    def testAddOperators(self):
        seq_R = robjects.r["seq"]
        mySeqA = seq_R(0, 3)
        mySeqB = seq_R(5, 7)
        mySeqAdd = mySeqA + mySeqB

        self.assertEqual(len(mySeqA)+len(mySeqB), len(mySeqAdd))

        for i, li in enumerate(mySeqA):
            self.assertEqual(li, mySeqAdd[i])       
        for j, li in enumerate(mySeqB):
            self.assertEqual(li, mySeqAdd[i+j+1])

    def testRAddOperators(self):
        seq_R = robjects.r["seq"]
        mySeq = seq_R(0, 10)
        mySeqAdd = mySeq.ro + 2
        for i, li in enumerate(mySeq):
            self.assertEqual(li + 2, mySeqAdd[i])

    def testRMultOperators(self):
        seq_R = robjects.r["seq"]
        mySeq = seq_R(0, 10)
        mySeqAdd = mySeq.ro + mySeq
        for i, li in enumerate(mySeq):
            self.assertEqual(li * 2, mySeqAdd[i])

    def testRPowerOperator(self):
        seq_R = robjects.r["seq"]
        mySeq = seq_R(0, 10)
        mySeqPow = mySeq.ro ** 2
        for i, li in enumerate(mySeq):
            self.assertEqual(li ** 2, mySeqPow[i])

 
    def testGetItem(self):
        letters = robjects.baseenv["letters"]
        self.assertEqual('a', letters[0])
        self.assertEqual('z', letters[25])

    def testGetItemOutOfBounds(self):
        letters = robjects.baseenv["letters"]
        self.assertRaises(IndexError, letters.__getitem__, 26)

    def testSetItem(self):
        vec = robjects.r.seq(1, 10)
        vec[0] = 20
        self.assertEqual(20, vec[0])

    def testSetItemOutOfBounds(self):
        vec = robjects.r.seq(1, 10)
        self.assertRaises(IndexError, vec.__setitem__, 20, 20)

    def getItemList(self):
        mylist = rlist(letters, "foo")
        idem = robjects.baseenv["identical"]
        self.assertTrue(idem(letters, mylist[0]))
        self.assertTrue(idem("foo", mylist[1]))

    def testGetNames(self):
        vec = robjects.Vector(array.array('i', [1,2,3]))
        v_names = [robjects.baseenv["letters"][x] for x in (0,1,2)]
        #FIXME: simplify this
        r_names = robjects.baseenv["c"](*v_names)
        vec = robjects.baseenv["names<-"](vec, r_names)
        for i in range(len(vec)):
            self.assertEqual(v_names[i], vec.names[i])

        vec.names[0] = 'x'

    def testSetNames(self):
        vec = robjects.Vector(array.array('i', [1,2,3]))
        names = ['x', 'y', 'z']
        vec.names = names
        for i in range(len(vec)):
            self.assertEqual(names[i], vec.names[i])

    def testNAInteger(self):
        vec = robjects.IntVector(range(3))
        vec[0] = robjects.NA_Integer
        self.assertTrue(robjects.baseenv['is.na'](vec)[0])
    def testNAReal(self):
        vec = robjects.FloatVector((1.0, 2.0, 3.0))
        vec[0] = robjects.NA_Real
        self.assertTrue(robjects.baseenv['is.na'](vec)[0])
    def testNALogical(self):
        vec = robjects.BoolVector((True, False, True))
        vec[0] = robjects.NA_Logical
        self.assertTrue(robjects.baseenv['is.na'](vec)[0])
    def testNAComplex(self):
        vec = robjects.ComplexVector((1+1j, 2+2j, 3+3j))
        vec[0] = robjects.NA_Complex
        self.assertTrue(robjects.baseenv['is.na'](vec)[0])
    def testNACharacter(self):
        vec = robjects.StrVector('abc')
        vec[0] = robjects.NA_Character
        self.assertTrue(robjects.baseenv['is.na'](vec)[0])

    def testRepr(self):
        vec = robjects.IntVector((1,2,3))
        s = repr(vec).split('\n')
        self.assertEqual('[1, 2, 3]', s[2])

    def testReprNonVectorInList(self):
        vec = robjects.ListVector(OrderedDict((('a', 1), 
                                               ('b', robjects.Formula('y ~ x')),
                                               )))
        s = repr(vec).split('\n')
        self.assertEqual('[IntVector, Formula]', s[2].strip())

    def testItems(self):
        vec = robjects.IntVector(range(3))
        vec.names = robjects.StrVector('abc')
        names = [k for k,v in vec.items()]
        self.assertEqual(['a', 'b', 'c'], names)
        values = [v for k,v in vec.items()]
        self.assertEqual([0, 1, 2], values)

    def testItemsNoNames(self):
        vec = robjects.IntVector(range(3))
        names = [k for k,v in vec.items()]
        self.assertEqual([None, None, None], names)
        values = [v for k,v in vec.items()]
        self.assertEqual([0, 1, 2], values)

class FactorVectorTestCase(unittest.TestCase):
    def testNew(self):
        vec = robjects.FactorVector(robjects.StrVector('abaabc'))
        self.assertEqual(6, len(vec))

    def testIsordered(self):
        vec = robjects.FactorVector(robjects.StrVector('abaabc'))
        self.assertFalse(vec.isordered)

    def testNlevels(self):
        vec = robjects.FactorVector(robjects.StrVector('abaabc'))
        self.assertEqual(3, vec.nlevels)

    def testLevels(self):
        vec = robjects.FactorVector(robjects.StrVector('abaabc'))
        self.assertEqual(3, len(vec.levels))
        self.assertEqual(set(('a','b','c')), set(tuple(vec.levels)))
    
    def testIter_labels(self):
        values = 'abaabc'
        vec = robjects.FactorVector(robjects.StrVector(values))
        it = vec.iter_labels()
        for a, b in zip(values, it):
            self.assertEqual(a, b)

    def testFactorWithAttrs(self):
        # issue #299
        r_src = """
        x <- factor(c("a","b","a"))
        attr(x, "foo") <- "bar"
        x
        """
        x = robjects.r(r_src)
        self.assertTrue('foo' in x.list_attrs())

_dateval_tuple = (1984, 1, 6, 6, 22, 0, 1, 6, 0) 

class DateTimeVectorTestCase(unittest.TestCase):
    
    def setUp(self):
        time.accept2dyear = False

    def tearDown(self):
        pass

    def testPOSIXlt_fromInvalidPythonTime(self):
        x = [time.struct_time(_dateval_tuple), 
             time.struct_time(_dateval_tuple)]
        x.append('foo')
        self.assertRaises(ValueError, robjects.POSIXlt, x)
        
    def testPOSIXlt_fromPythonTime(self):
        x = [time.struct_time(_dateval_tuple), 
             time.struct_time(_dateval_tuple)]
        res = robjects.POSIXlt(x)
        self.assertEqual(2, len(x))

    def testPOSIXct_fromInvalidPythonTime(self):
        x = [time.struct_time(_dateval_tuple), 
             time.struct_time(_dateval_tuple)]
        x.append('foo')
        # string 'foo' does not have attribute 'tm_zone'  
        self.assertRaises(AttributeError, robjects.POSIXct, x)

    def testPOSIXct_fromPythonTime(self):
        x = [time.struct_time(_dateval_tuple), 
             time.struct_time(_dateval_tuple)]
        res = robjects.POSIXct(x)
        self.assertEqual(2, len(x))

    def testPOSIXct_fromPythonDatetime(self):
        x = [datetime.datetime(*_dateval_tuple[:-2]), 
             datetime.datetime(*_dateval_tuple[:-2])]
        res = robjects.POSIXct(x)
        self.assertEqual(2, len(x))

    def testPOSIXct_fromSexp(self):
        sexp = robjects.r('ISOdate(2013, 12, 11)')
        res = robjects.POSIXct(sexp)
        self.assertEqual(1, len(res))

class ExtractDelegatorTestCase(unittest.TestCase):

    def setUp(self):
        self.console = robjects.rinterface.get_writeconsole_regular()

    def tearDown(self):
        robjects.rinterface.set_writeconsole_regular(self.console)

        
    def testFloorDivision(self):
        v = robjects.vectors.IntVector((2,3,4))
        if sys.version_info[0] == 2:
            res = v.ro / 2
        else:
            res = v.ro // 2
        self.assertEqual((1,1,2), tuple(int(x) for x in res))
            
    def testExtractByIndex(self):
        seq_R = robjects.baseenv["seq"]
        mySeq = seq_R(0, 10)
        # R indexing starts at one
        myIndex = robjects.Vector(array.array('i', range(1, 11, 2)))

        mySubset = mySeq.rx(myIndex)
        for i, si in enumerate(myIndex):
            self.assertEqual(mySeq[si-1], mySubset[i])
        
    def testExtractByName(self):
        seq_R = robjects.baseenv["seq"]
        mySeq = seq_R(0, 25)

        letters = robjects.baseenv["letters"]
        mySeq = robjects.baseenv["names<-"](mySeq, 
                                                     letters)

        # R indexing starts at one
        myIndex = robjects.Vector(letters[2])

        mySubset = mySeq.rx(myIndex)

        for i, si in enumerate(myIndex):
            self.assertEqual(2, mySubset[i])

    def testExtractIndexError(self):
        seq_R = robjects.baseenv["seq"]
        mySeq = seq_R(0, 10)
        # R indexing starts at one
        myIndex = robjects.Vector(['a', 'b', 'c'])

        def noconsole(x):
            pass
        robjects.rinterface.set_writeconsole_regular(noconsole)

        self.assertRaises(ri.RRuntimeError, mySeq.rx, myIndex)


       
    def testReplace(self):
        vec = robjects.IntVector(range(1, 6))
        i = array.array('i', [1, 3])
        vec.rx[rlc.TaggedList((i, ))] = 20
        self.assertEqual(20, vec[0])
        self.assertEqual(2, vec[1])
        self.assertEqual(20, vec[2])
        self.assertEqual(4, vec[3])

        vec = robjects.IntVector(range(1, 6))
        i = array.array('i', [1, 5])
        vec.rx[rlc.TaggedList((i, ))] = 50
        self.assertEqual(50, vec[0])
        self.assertEqual(2, vec[1])
        self.assertEqual(3, vec[2])
        self.assertEqual(4, vec[3])
        self.assertEqual(50, vec[4])

        vec = robjects.IntVector(range(1, 6))
        vec.rx[1] = 70
        self.assertEqual(70, vec[0])
        self.assertEqual(2, vec[1])
        self.assertEqual(3, vec[2])
        self.assertEqual(4, vec[3])
        self.assertEqual(5, vec[4])

        vec = robjects.IntVector(range(1, 6))
        vec.rx[robjects.IntVector((1, 3))] = 70
        self.assertEqual(70, vec[0])
        self.assertEqual(2, vec[1])
        self.assertEqual(70, vec[2])
        self.assertEqual(4, vec[3])
        self.assertEqual(5, vec[4])


        m = robjects.r('matrix(1:10, ncol=2)')
        m.rx[1, 1] = 9
        self.assertEqual(9, m[0])

        m = robjects.r('matrix(1:10, ncol=2)')
        m.rx[2, robjects.IntVector((1,2))] = 9
        self.assertEqual(9, m[1])
        self.assertEqual(9, m[6])
                                 
    def testExtractRecyclingRule(self):
        # recycling rule
        v = robjects.Vector(array.array('i', range(1, 23)))
        m = robjects.r.matrix(v, ncol = 2)
        col = m.rx(True, 1)
        self.assertEqual(11, len(col))

    def testExtractList(self):
        # list
        letters = robjects.baseenv["letters"]
        myList = rlist(l=letters, f="foo")
        idem = robjects.baseenv["identical"]
        self.assertTrue(idem(letters, myList.rx("l")[0]))
        self.assertTrue(idem("foo", myList.rx("f")[0]))


class ConversionHelperTestCase(unittest.TestCase):

    def testSequenceToVector(self):
        res = robjects.sequence_to_vector((1,2,3))
        self.assertTrue(isinstance(res, robjects.IntVector))

        res = robjects.sequence_to_vector((1,2,3.0))
        self.assertTrue(isinstance(res, robjects.FloatVector))

        res = robjects.sequence_to_vector((1,2,'a'))
        self.assertTrue(isinstance(res, robjects.StrVector))

        self.assertRaises(ValueError, robjects.sequence_to_vector, list())

def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(VectorTestCase)
    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(FactorVectorTestCase))
    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(DateTimeVectorTestCase))
    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(ExtractDelegatorTestCase))
    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(ConversionHelperTestCase))

    return suite

if __name__ == '__main__':
     unittest.main()
