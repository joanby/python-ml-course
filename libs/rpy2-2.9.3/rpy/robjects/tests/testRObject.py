import unittest
import rpy2.robjects as robjects
rinterface = robjects.rinterface
import array
import tempfile

class RObjectTestCase(unittest.TestCase):
    
    def testNew(self):

        identical = rinterface.baseenv["identical"]
        py_a = array.array('i', [1,2,3])
        self.assertRaises(ValueError, robjects.RObject, py_a)
        
        ri_v = rinterface.SexpVector(py_a, rinterface.INTSXP)
        ro_v = robjects.RObject(ri_v)

        self.assertTrue(identical(ro_v, ri_v)[0])

        del(ri_v)
        self.assertEqual(rinterface.INTSXP, ro_v.typeof)

    def testR_repr(self):
        obj = robjects.baseenv["pi"]
        s = obj.r_repr()
        self.assertTrue(s.startswith('3.14'))


    def testStr(self):
        prt = robjects.baseenv["pi"]
        s = prt.__str__()
        self.assertTrue(s.startswith('[1] 3.14'))


    def testRclass(self):
        self.assertEqual("character",
                          robjects.baseenv["letters"].rclass[0])
        self.assertEqual("numeric",
                          robjects.baseenv["pi"].rclass[0])
        self.assertEqual("function",
                          robjects.globalenv.get("help").rclass[0])

    def testRclass_set(self):
        x = robjects.r("1:3")
        old_class = x.rclass
        x.rclass = robjects.StrVector(("Foo", )) + x.rclass
        self.assertEqual("Foo",
                          x.rclass[0])
        self.assertEqual(old_class[0], x.rclass[1])

    def testRclass_set_usingstring(self):
        x = robjects.r("1:3")
        old_class = x.rclass
        x.rclass = "Foo"
        self.assertEqual("Foo",
                          x.rclass[0])
        
    def testDo_slot(self):
        self.assertEqual("A1.4, p. 270",
                          robjects.globalenv.get("BOD").do_slot("reference")[0])

    def testSlots(self):
        x = robjects.r('list(a=1,b=2,c=3)')
        s = x.slots
        self.assertEqual(1, len(s))
        self.assertEqual(('names', ), tuple(s.keys()))
        self.assertEqual(('a', 'b', 'c'), tuple(s['names']))

        s['names'] = 0
        self.assertEqual(1, len(s))
        self.assertEqual(('names', ), tuple(s.keys()))
        self.assertEqual((0, ), tuple(s['names']))
        
import pickle

class RObjectPicklingTestCase(unittest.TestCase):
    
    def testPickle(self):
        tmp_file = tempfile.NamedTemporaryFile()
        robj = robjects.baseenv["pi"]
        pickle.dump(robj, tmp_file)
        tmp_file.flush()
        tmp_file.seek(0)
        robj_again = pickle.load(tmp_file)
        tmp_file.close()

        self.assertTrue(isinstance(robj, robjects.FloatVector))

        # Check that underlying R objects are identical.
        self.assertTrue(robjects.baseenv["identical"](robj,
                                                      robj_again)[0])
        # Check the instance dict is also identical
        self.assertSetEqual(set(robj.__dict__.keys()),
                            set(robj_again.__dict__.keys())) 


import rpy2.robjects.methods

class RS4TestCase(unittest.TestCase):
    def setUp(self):
        robjects.r('setClass("A", representation(a="numeric", b="character"))')
        
    def tearDown(self):
        robjects.r('setClass("A")')
        
    def testSlotNames(self):
        ainstance = robjects.r('new("A", a=1, b="c")')
        self.assertEqual(('a', 'b'), tuple(ainstance.slotnames()))

    def testIsClass(self):
        ainstance = robjects.r('new("A", a=1, b="c")')
        self.assertFalse(ainstance.isclass("B"))
        self.assertTrue(ainstance.isclass("A"))

    def testValidObject(self):
        ainstance = robjects.r('new("A", a=1, b="c")')
        self.assertTrue(ainstance.validobject())
        #FIXME: test invalid objects ?

def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(RObjectTestCase)
    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(RObjectPicklingTestCase))
    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(RS4TestCase))
    return suite

if __name__ == '__main__':
     unittest.main()
