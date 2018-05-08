import unittest
import rpy2.rinterface as rinterface

rinterface.initr()

class SexpExtPtrTestCase(unittest.TestCase):

    def setUp(self):
        self.console = rinterface.get_writeconsole_regular()
        def noconsole(x):
            pass
        rinterface.set_writeconsole_regular(noconsole)

    def tearDown(self):
        rinterface.set_writeconsole_regular(self.console)

    def testNewDefault(self):
        pyobject = "ahaha"
        sexp_new = rinterface.SexpExtPtr(pyobject)
        # R External pointer are never copied
        self.assertEqual(rinterface.EXTPTRSXP, sexp_new.typeof)

    def testNewTag(self):
        pyobject = "ahaha"
        sexp_new = rinterface.SexpExtPtr(pyobject, 
                                         tag = rinterface.StrSexpVector("b"))
        self.assertEqual(rinterface.EXTPTRSXP, sexp_new.typeof)
        self.assertEqual('b', sexp_new.__tag__[0])

    def testNewInvalidTag(self):
        pyobject = "ahaha"
        self.assertRaises(TypeError, rinterface.SexpExtPtr,
                          pyobject, tag = True)

    def testNewProtected(self):
        pyobject = "ahaha"
        sexp_new = rinterface.SexpExtPtr(pyobject, 
                                         protected = rinterface.StrSexpVector("c"))
        self.assertEqual(rinterface.EXTPTRSXP, sexp_new.typeof)
        self.assertEqual('c', sexp_new.__protected__[0])

    def testNewInvalidProtected(self):
        pyobject = "ahaha"
        self.assertRaises(TypeError, rinterface.SexpExtPtr,
                          pyobject, protected = True)


def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(SexpExtPtrTestCase)
    return suite

if __name__ == '__main__':
    tr = unittest.TextTestRunner(verbosity = 2)
    tr.run(suite())

