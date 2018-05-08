import unittest
import rpy2.robjects as robjects
rinterface = robjects.rinterface
import array

class EnvironmentTestCase(unittest.TestCase):
    def testNew(self):
        env = robjects.Environment()
        self.assertEqual(rinterface.ENVSXP, env.typeof)

    def testNewValueError(self):
        self.assertRaises(ValueError, robjects.Environment, 'a')

    def testSetItem(self):
        env = robjects.Environment()
        env['a'] = 123
        self.assertTrue('a' in env)

    def testKeys(self):
        env = robjects.Environment()
        env['a'] = 123
        keys = list(env.keys())
        self.assertEquals(1, len(keys))
        keys.sort()
        for it_a, it_b in zip(keys,
                              ('a',)):
            self.assertEquals(it_a, it_b)
        
    def testItems(self):
        env = robjects.Environment()
        env['a'] = 123
        items = list(env.items())
        self.assertEquals(1, len(items))
        items.sort(key=lambda x: x[0])
        for it_a, it_b in zip(items,
                              (('a', 123),)):
            self.assertEquals(it_a[0], it_b[0])
            self.assertEquals(it_a[1][0], it_b[1])
        
def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(EnvironmentTestCase)
    return suite

if __name__ == '__main__':
     unittest.main()
