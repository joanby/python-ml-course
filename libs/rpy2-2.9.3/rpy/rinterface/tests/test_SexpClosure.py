# coding: utf-8
import unittest
import sys
import rpy2.rinterface as rinterface
import rpy2.rlike.container as rlc

rinterface.initr()


class SexpClosureTestCase(unittest.TestCase):


    def setUp(self):
        self.console = rinterface.get_writeconsole_regular()
        def noconsole(x):
            pass
        rinterface.set_writeconsole_regular(noconsole)

    def tearDown(self):
        rinterface.set_writeconsole_regular(self.console)

    def testNew(self):
        x = "a"
        self.assertRaises(ValueError, rinterface.SexpClosure, x)
        
    def testTypeof(self):
        sexp = rinterface.globalenv.get("plot")
        self.assertEqual(sexp.typeof, rinterface.CLOSXP)

    def testRError(self):
        r_sum = rinterface.baseenv["sum"]
        letters = rinterface.baseenv["letters"]
        self.assertRaises(rinterface.RRuntimeError, r_sum, letters)

    def testUTF8params(self):
        c = rinterface.globalenv.get('c')
        d = dict([(u'哈哈', 1)])
        res = c(**d)
        self.assertEqual(u'哈哈', res.do_slot("names")[0])

    def testEmptystringparams(self):
        d = dict([('', 1)])
        self.assertRaises(TypeError, rinterface.baseenv['list'], **d)

    def testClosureenv(self):
        exp = rinterface.parse("function(x) { x[y] }")
        fun = rinterface.baseenv["eval"](exp)
        vec = rinterface.baseenv["letters"]
        self.assertRaises(rinterface.RRuntimeError, fun, vec)

        fun.closureenv["y"] = (rinterface
                               .SexpVector([1, ], 
                                           rinterface.INTSXP))
        self.assertEqual('a', fun(vec)[0])
        
        fun.closureenv["y"] = (rinterface
                               .SexpVector([2, ], 
                                           rinterface.INTSXP))
        self.assertEqual('b', fun(vec)[0])

    def testCallS4SetClass(self):
        # R's package "methods" can perform uncommon operations
        r_setClass = rinterface.globalenv.get('setClass')
        r_representation = rinterface.globalenv.get('representation')
        attrnumeric = rinterface.SexpVector(["numeric", ],
                                            rinterface.STRSXP)
        classname = rinterface.SexpVector(['Track', ], rinterface.STRSXP)
        classrepr = r_representation(x = attrnumeric,
                                     y = attrnumeric)
        r_setClass(classname,
                   classrepr)



    def testRcallOrdDict(self):
        ad = rlc.OrdDict((('a', rinterface.SexpVector([2, ], 
                                                      rinterface.INTSXP)), 
                          ('b', rinterface.SexpVector([1, ], 
                                                      rinterface.INTSXP)),
                          (None, rinterface.SexpVector([5, ], 
                                                       rinterface.INTSXP)),
                          ('c', rinterface.SexpVector([0, ], 
                                                      rinterface.INTSXP))))

        mylist = rinterface.baseenv['list'].rcall(tuple(ad.items()), 
                                                  rinterface.globalenv)
        
        names = [x for x in mylist.do_slot("names")]
        
        for i in range(4):
            self.assertEqual(('a', 'b', '', 'c')[i], names[i])

    def testRcallOrdDictEnv(self):
        ad = rlc.OrdDict( ((None, rinterface.parse('sum(x)')),) )
        env_a = rinterface.baseenv['new.env']()
        env_a['x'] = rinterface.IntSexpVector([1,2,3])
        sum_a = rinterface.baseenv['eval'].rcall(tuple(ad.items()), 
                                                 env_a)
        self.assertEqual(6, sum_a[0])
        env_b = rinterface.baseenv['new.env']()
        env_b['x'] = rinterface.IntSexpVector([4,5,6])
        sum_b = rinterface.baseenv['eval'].rcall(tuple(ad.items()), 
                                                 env_b)
        self.assertEqual(15, sum_b[0])        
        
    def testErrorInCall(self):
        mylist = rinterface.baseenv['list']
        
        self.assertRaises(ValueError, mylist, 'foo')

    def testMissingArg(self):
        exp = rinterface.parse("function(x) { missing(x) }")
        fun = rinterface.baseenv["eval"](exp)
        nonmissing = rinterface.SexpVector([0, ], rinterface.INTSXP)
        missing = rinterface.MissingArg
        self.assertEqual(False, fun(nonmissing)[0])
        self.assertEqual(True, fun(missing)[0])

    def testScalarConvertInteger(self):
        self.assertEqual('integer',
                          rinterface.baseenv["typeof"](int(1))[0])

    def testScalarConvertDouble(self):
        self.assertEqual('double', 
                          rinterface.baseenv["typeof"](1.0)[0])

    def testScalarConvertBoolean(self):
        self.assertEqual('logical', 
                          rinterface.baseenv["typeof"](True)[0])
        

def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(SexpClosureTestCase)
    return suite

if __name__ == '__main__':
    tr = unittest.TextTestRunner(verbosity = 2)
    tr.run(suite())
    
