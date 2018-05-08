import unittest
import rpy2.robjects as robjects
rinterface = robjects.rinterface

class FormulaTestCase(unittest.TestCase):

    def testNew(self):
        fml = robjects.Formula("y ~ x")
        self.assertEqual("formula", fml.rclass[0])
        
    def testGetenvironment(self):
        fml = robjects.Formula("y ~ x")
        env = fml.getenvironment()
        self.assertEqual("environment", env.rclass[0])

    def testSetenvironment(self):
        fml = robjects.Formula("y ~ x")
        newenv = robjects.baseenv['new.env']()
        env = fml.getenvironment()
        self.assertFalse(newenv.rsame(env))
        fml.setenvironment(newenv)
        env = fml.getenvironment()
        self.assertTrue(newenv.rsame(env))

def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(FormulaTestCase)
    return suite

if __name__ == '__main__':
     unittest.main()
