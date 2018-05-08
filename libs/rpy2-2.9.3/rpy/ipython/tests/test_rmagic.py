import unittest
from itertools import product

# Currently numpy is a testing requirement, but rpy2 should work without numpy
try:
    import numpy as np
    has_numpy = True
except:
    has_numpy = False
try:
    import pandas as pd
    has_pandas = True
except:
    has_pandas = False
from IPython.testing.globalipapp import get_ipython
from IPython.utils.py3compat import PY3

if PY3:
    from io import StringIO
    np_string_type = 'U'
else:
    from StringIO import StringIO
    np_string_type = 'S'

from rpy2.ipython import rmagic

# from IPython.core.getipython import get_ipython
from rpy2 import rinterface
from rpy2.robjects import r, vectors, globalenv
import rpy2.robjects.packages as rpacks

class TestRmagic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        '''Set up an IPython session just once.
        It'd be safer to set it up for each test, but for now, I'm mimicking the
        IPython team's logic.
        '''
        cls.ip = get_ipython()
        # This is just to get a minimally modified version of the changes
        # working
        cls.ip.magic('load_ext rpy2.ipython')

    def setUp(self):
        if hasattr(rmagic.template_converter, 'activate'):
            rmagic.template_converter.activate()
    def tearDown(self):
        # This seems like the safest thing to return to a safe state
        self.ip.run_line_magic('Rdevice', 'png')
        if hasattr(rmagic.template_converter, 'deactivate'):
            rmagic.template_converter.deactivate()

    @unittest.skipIf(not has_numpy, 'numpy not installed')
    def test_push(self):
        self.ip.push({'X':np.arange(5), 'Y':np.array([3,5,4,6,7])})
        self.ip.run_line_magic('Rpush', 'X Y')
        np.testing.assert_almost_equal(np.asarray(r('X')), self.ip.user_ns['X'])
        np.testing.assert_almost_equal(np.asarray(r('Y')), self.ip.user_ns['Y'])

    @unittest.skipIf(not has_numpy, 'numpy not installed')
    def test_push_localscope(self):
        """Test that Rpush looks for variables in the local scope first."""

        self.ip.run_cell('''
def rmagic_addone(u):
    %Rpush u
    %R result = u+1
    %Rpull result
    return result[0]
u = 0
result = rmagic_addone(12344)
''')
        result = self.ip.user_ns['result']
        np.testing.assert_equal(result, 12345)

    @unittest.skipUnless(has_pandas, 'pandas is not available in python')
    @unittest.skipIf(not has_numpy, 'numpy not installed')
    def test_push_dataframe(self):
        df = pd.DataFrame([{'a': 1, 'b': 'bar'}, {'a': 5, 'b': 'foo', 'c': 20}])
        self.ip.push({'df':df})
        self.ip.run_line_magic('Rpush', 'df')

        # This is converted to factors, which are currently converted back to Python
        # as integers, so for now we test its representation in R.
        sio = StringIO()
        rinterface.set_writeconsole_regular(sio.write)
        try:
            r('print(df$b[1])')
            self.assertIn('[1] "bar"', sio.getvalue())
        finally:
            rinterface.set_writeconsole_regular(None)

        # Values come packaged in arrays, so we unbox them to test.
        self.assertEqual(r('df$a[2]')[0], 5)
        missing = r('df$c[1]')[0]
        assert np.isnan(missing), missing

    @unittest.skipIf(not has_numpy, 'numpy not installed')
    def test_pull(self):
        r('Z=c(11:20)')
        self.ip.run_line_magic('Rpull', 'Z')
        np.testing.assert_almost_equal(np.asarray(r('Z')), self.ip.user_ns['Z'])
        np.testing.assert_almost_equal(self.ip.user_ns['Z'], np.arange(11,21))

    @unittest.skipIf(not has_numpy, 'numpy not installed')
    def test_Rconverter(self):
        # If we get to dropping numpy requirement, we might use something
        # like the following:
        # self.assertSequenceEqual(buffer(a).buffer_info(),
        #                          buffer(b).buffer_info())

        # numpy recarray (numpy's version of a data frame)
        dataf_np= np.array([(1, 2.9, 'a'), (2, 3.5, 'b'), (3, 2.1, 'c')],
                           dtype=[('x', '<i4'), ('y', '<f8'), ('z', '|%s1' % np_string_type)])
        # store it in the notebook's user namespace
        self.ip.user_ns['dataf_np'] = dataf_np
        # equivalent to:
        #     %Rpush dataf_np
        # that is send Python object 'dataf_np' into R's globalenv
        # as 'dataf_np'. The current conversion rules will make it an
        # R data frame.
        self.ip.run_line_magic('Rpush', 'dataf_np')

        # Now retreive 'dataf_np' from R's globalenv. Twice because
        # we want to test whether copies are made
        fromr_dataf_np = self.ip.run_line_magic('Rget', 'dataf_np')
        fromr_dataf_np_again = self.ip.run_line_magic('Rget', 'dataf_np')

        # check whether the data frame retrieved has the same content
        # as the original recarray
        self.assertEqual(len(dataf_np), len(fromr_dataf_np))
        for col_i, col_n in enumerate(('x', 'y')):
            if has_pandas:
                self.assertTrue(isinstance(fromr_dataf_np, pd.DataFrame))
                self.assertSequenceEqual(tuple(dataf_np[col_i]),
                                         tuple(fromr_dataf_np.iloc[col_i].values))
            else:
                # has_numpy then
                self.assertSequenceEqual(tuple(dataf_np[col_i]),
                                         tuple(fromr_dataf_np[col_i]))

        # pandas2ri is currently making copies
        # # modify the data frame retrieved to check whether
        # # a copy was made
        # fromr_dataf_np['x'].values[0] = 11
        # self.assertEqual(11, fromr_dataf_np_again['x'][0])
        # fromr_dataf_np['x'].values[0] = 1
        
        # retrieve `dataf_np` from R into `fromr_dataf_np` in the notebook. 
        self.ip.run_cell_magic('R',
                               '-o dataf_np',
                               'dataf_np')

        dataf_np_roundtrip = self.ip.user_ns['dataf_np']
        self.assertSequenceEqual(tuple(fromr_dataf_np['x']),
                                 tuple(dataf_np_roundtrip['x']))
        self.assertSequenceEqual(tuple(fromr_dataf_np['y']),
                                 tuple(dataf_np_roundtrip['y']))
        
    @unittest.skipIf(not has_numpy, 'numpy not installed')
    def test_cell_magic(self):
        self.ip.push({'x':np.arange(5), 'y':np.array([3,5,4,6,7])})
        # For now, print statements are commented out because they print
        # erroneous ERRORs when running via rpy2.tests
        snippet = '''
        print(summary(a))
        plot(x, y, pch=23, bg='orange', cex=2)
        plot(x, x)
        print(summary(x))
        r = resid(a)
        xc = coef(a)
        '''
        self.ip.run_cell_magic('R', '-i x,y -o r,xc -w 150 -u mm a=lm(y~x)',
                               snippet)
        np.testing.assert_almost_equal(self.ip.user_ns['xc'], [3.2, 0.9])
        np.testing.assert_almost_equal(self.ip.user_ns['r'], np.array([-0.2,  0.9, -1. ,  0.1,  0.2]))

    def test_cell_magic_localconverter(self):

        x = (1,2,3)
        from rpy2.rinterface import StrSexpVector
        def tuple_str(tpl):
            res = StrSexpVector(tpl)
            return res
        from rpy2.robjects.conversion import Converter
        my_converter = Converter('my converter')
        my_converter.py2ri.register(tuple, tuple_str)
        from rpy2.robjects import default_converter

        foo = default_converter + my_converter
        
        self.ip.push({'x':x,
                      'foo': foo})
        snippet = '''
        x
        '''
        self.assertRaises(NotImplementedError,
                          self.ip.run_cell_magic,
                          'R', '-i x', snippet)
        self.ip.run_cell_magic('R', '-i x -c foo',
                               snippet)
        self.assertTrue(isinstance(globalenv['x'],
                                   vectors.StrVector))
        
    def test_rmagic_localscope(self):
        self.ip.push({'x':0})
        self.ip.run_line_magic('R', '-i x -o result result <-x+1')
        result = self.ip.user_ns['result']
        self.assertEqual(result[0], 1)

        self.ip.run_cell('''def rmagic_addone(u):
        %R -i u -o result result <- u+1
        return result[0]''')
        self.ip.run_cell('result = rmagic_addone(1)')
        result = self.ip.user_ns['result']
        self.assertEqual(result, 2)

        self.assertRaises(
            NameError,
            self.ip.run_line_magic,
            "R",
            "-i var_not_defined 1+1")

    @unittest.skipIf(not has_numpy, 'numpy not installed')
    def test_png_plotting_args(self):
        '''Exercise the PNG plotting machinery'''

        self.ip.push({'x':np.arange(5), 'y':np.array([3,5,4,6,7])})

        cell = '''
        plot(x, y, pch=23, bg='orange', cex=2)
        '''

        png_px_args = [' '.join(('--units=px',w,h,p)) for 
                       w, h, p in product(['--width=400 ',''],
                                          ['--height=400',''],
                                          ['-p=10', ''])]

        for line in png_px_args:
            self.ip.run_line_magic('Rdevice', 'png')
            self.ip.run_cell_magic('R', line, cell)

    @unittest.skipUnless(rpacks.isinstalled('Cairo'), 'Cairo not installed')
    def test_svg_plotting_args(self):
        '''Exercise the plotting machinery

        To pass SVG tests, we need Cairo installed in R.'''
        self.ip.push({'x':np.arange(5), 'y':np.array([3,5,4,6,7])})

        cell = '''
        plot(x, y, pch=23, bg='orange', cex=2)
        '''

        basic_args = [' '.join((w,h,p)) for w, h, p in product(['--width=6 ',''],
                                                               ['--height=6',''],
                                                               ['-p=10', ''])]

        for line in basic_args:
            self.ip.run_line_magic('Rdevice', 'svg')
            self.ip.run_cell_magic('R', line, cell)

        png_args = ['--units=in --res=1 ' + s for s in basic_args]
        for line in png_args:
            self.ip.run_line_magic('Rdevice', 'png')
            self.ip.run_cell_magic('R', line, cell)

    @unittest.skip('Test for X11 skipped.')
    def test_plotting_X11(self):
        self.ip.push({'x':np.arange(5), 'y':np.array([3,5,4,6,7])})

        cell = '''
        plot(x, y, pch=23, bg='orange', cex=2)
        '''
        self.ip.run_line_magic('Rdevice', 'X11')
        self.ip.run_cell_magic('R', '', cell)

if __name__ == '__main__':
    unittest.main()        
