import unittest
import rpy2.robjects as robjects
import rpy2.robjects.language as lg
rinterface = robjects.rinterface


class LanguageTestCase(unittest.TestCase):

    def setUp(self):
        for var in ('x', 'y'):
            if var in robjects.globalenv.keys():
                #del(ro.globalenv[var]) #segfault
                pass

    def tearDown(self):
        for var in ('x', 'y'):
            if var in robjects.globalenv.keys():
                #del(ro.globalenv[var]) #segfault
                pass

    def testEval(self):
        code = """
x <- 1+2
y <- (x+1) / 2
"""
        res = lg.eval(code)
        self.assertTrue('x' in robjects.globalenv.keys())
        self.assertEqual(3, robjects.globalenv['x'][0])
        self.assertTrue('y' in robjects.globalenv.keys())
        self.assertEqual(2, robjects.globalenv['y'][0])

    def testEvalInEnvironment(self):
        code = """
x <- 1+2
y <- (x+1) / 2
"""
        env = robjects.Environment()
        res = lg.eval(code, envir=env)
        self.assertTrue('x' in env.keys())
        self.assertEqual(3, env['x'][0])
        self.assertTrue('y' in env.keys())
        self.assertEqual(2, env['y'][0])

def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(LanguageTestCase)
    return suite

if __name__ == '__main__':
     unittest.main()
