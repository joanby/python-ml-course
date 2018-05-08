# coding: utf-8
import unittest
import sys
import rpy2.rinterface as rinterface

rinterface.initr()

class SexpEnvironmentTestCase(unittest.TestCase):

    def setUp(self):
        self.console = rinterface.get_writeconsole_regular()
        def noconsole(x):
            pass
        rinterface.set_writeconsole_regular(noconsole)

    def tearDown(self):
        rinterface.set_writeconsole_regular(self.console)

    def testNew(self):
        sexp = rinterface.globalenv
        sexp_new = rinterface.SexpEnvironment(sexp)

        idem = rinterface.globalenv.get("identical")
        self.assertTrue(idem(sexp, sexp_new)[0])

        sexp_new2 = rinterface.Sexp(sexp)
        self.assertTrue(idem(sexp, sexp_new2)[0])
        del(sexp)
        self.assertTrue(idem(sexp_new, sexp_new2)[0])

        self.assertRaises(ValueError, rinterface.SexpEnvironment, '2')

    def testGlobalEnv(self):
        ok = isinstance(rinterface.globalenv, rinterface.SexpEnvironment) 
        self.assertTrue(ok)

    def testGetClosure(self):
        help_R = rinterface.globalenv.get("help")
        ok = isinstance(help_R, rinterface.SexpClosure)
        self.assertTrue(ok)

    def testGetVector(self):
        pi_R = rinterface.globalenv.get("pi")
        ok = isinstance(pi_R, rinterface.SexpVector)
        self.assertTrue(ok)

    def testGetEnvironment(self):
        ge_R = rinterface.globalenv.get(".GlobalEnv")
        ok = isinstance(ge_R, rinterface.SexpEnvironment)
        self.assertTrue(ok)

    def testGetOnlyFromLoadedLibrary(self):
        self.assertRaises(LookupError, rinterface.globalenv.get, "survfit")
        rinterface.globalenv.get("library")(rinterface.StrSexpVector(["survival", ]))
        sfit_R = rinterface.globalenv.get("survfit")
        ok = isinstance(sfit_R, rinterface.SexpClosure)
        self.assertTrue(ok)


    def testGet_functionOnly_lookupError(self):
        # now with the function-only option

        self.assertRaises(LookupError, 
                          rinterface.globalenv.get, "pi", wantfun = True)

    def testGet_functionOnly(self):
        hist = rinterface.globalenv.get("hist", wantfun = False)
        self.assertEqual(rinterface.CLOSXP, hist.typeof)
        rinterface.globalenv["hist"] = rinterface.SexpVector(["foo", ], 
                                                             rinterface.STRSXP)

        hist = rinterface.globalenv.get("hist", wantfun = True)
        self.assertEqual(rinterface.CLOSXP, hist.typeof)
        
    def testGet_emptyString(self):
        self.assertRaises(ValueError, rinterface.globalenv.get, "")

    def testSubscript(self):
        ge = rinterface.globalenv
        obj = rinterface.globalenv.get("letters")
        ge["a"] = obj
        a = rinterface.globalenv["a"]
        ok = ge.get("identical")(obj, a)
        self.assertTrue(ok[0])

    def testSubscript_utf8(self):
        env = rinterface.baseenv['new.env']()
        env[u'呵呵'] = rinterface.IntSexpVector((1,))
        self.assertEqual(1, len(env))
        self.assertEqual(1, len(env[u'呵呵']))
        self.assertEqual(1, env[u'呵呵'][0])

    def testSubscript_missing_utf8(self):
        env = rinterface.baseenv['new.env']()
        self.assertRaises(KeyError, env.__getitem__, u'呵呵')
        
    def testSubscript_no_utf8(self):
        env = rinterface.baseenv['new.env']()
        self.assertRaises(KeyError, env.__getitem__, u'x')
        
    def testSubscript_emptyString(self):
        ge = rinterface.globalenv
        self.assertRaises(KeyError, ge.__getitem__, "")

    def testLength(self):
        newEnv = rinterface.globalenv.get("new.env")
        env = newEnv()
        self.assertEqual(0, len(env))
        env["a"] = rinterface.SexpVector([123, ], rinterface.INTSXP)
        self.assertEqual(1, len(env))
        env["b"] = rinterface.SexpVector([123, ], rinterface.INTSXP)
        self.assertEqual(2, len(env))

    def testIter(self):
        newEnv = rinterface.globalenv.get("new.env")
        env = newEnv()
        env["a"] = rinterface.SexpVector([123, ], rinterface.INTSXP)
        env["b"] = rinterface.SexpVector([456, ], rinterface.INTSXP)
        symbols = [x for x in env]
        self.assertEqual(2, len(symbols))
        for s in ["a", "b"]:
            self.assertTrue(s in symbols)

    def testKeys(self):
        newEnv = rinterface.globalenv.get("new.env")
        env = newEnv()
        env["a"] = rinterface.SexpVector([123, ], rinterface.INTSXP)
        env["b"] = rinterface.SexpVector([456, ], rinterface.INTSXP)
        symbols = env.keys()
        self.assertEqual(2, len(symbols))
        for s in ["a", "b"]:
            self.assertTrue(s in symbols)

    def testDel(self):
        env = rinterface.globalenv.get("new.env")()
        env["a"] = rinterface.SexpVector([123, ], rinterface.INTSXP)
        env["b"] = rinterface.SexpVector([456, ], rinterface.INTSXP)
        self.assertEqual(2, len(env))
        del(env['a'])
        self.assertEqual(1, len(env))
        self.assertTrue('b' in env)

    def testDelKeyError(self):
        self.assertRaises(KeyError, rinterface.globalenv.__delitem__, 'foo')

    def testDelBaseError(self):
        self.assertRaises(ValueError, rinterface.baseenv.__delitem__, 'letters')

def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(SexpEnvironmentTestCase)
    return suite

if __name__ == '__main__':
    tr = unittest.TextTestRunner(verbosity = 2)
    tr.run(suite())

