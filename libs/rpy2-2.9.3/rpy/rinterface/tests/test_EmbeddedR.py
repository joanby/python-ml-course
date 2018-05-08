import unittest
import pickle
import multiprocessing
import rpy2
import rpy2.rinterface as rinterface
import sys, os, subprocess, time, tempfile, io, signal, gc

rinterface.initr()




def onlyAQUAorWindows(function):
    def res(self):
        platform = rinterface.baseenv.get('.Platform')
        platform_gui = [e for i, e in enumerate(platform.do_slot('names')) if e == 'GUI'][0]
        platform_ostype = [e for i, e in enumerate(platform.do_slot('names')) if e == 'OS.type'][0]
        if (platform_gui != 'AQUA') and (platform_ostype != 'windows'):
            self.assertTrue(False) # cannot be tested outside GUI==AQUA or OS.type==windows
            return None
        else:
            return function(self)

class CustomException(Exception):
    pass

class EmbeddedRTestCase(unittest.TestCase):


    def testConsolePrint(self):
        if sys.version_info[0] == 3:
            tmp_file = io.StringIO()
            stdout = sys.stdout
            sys.stdout = tmp_file
            try:
                rinterface.consolePrint('haha')
            except Exception as e:
                sys.stdout = stdout
                raise e
            sys.stdout = stdout
            tmp_file.flush()
            tmp_file.seek(0)
            self.assertEqual('haha', ''.join(s for s in tmp_file).rstrip())
            tmp_file.close()
        else:
            # no need to test which Python 2, only 2.7 supported
            with tempfile.NamedTemporaryFile() as tmp_file:
                stdout = sys.stdout
                sys.stdout = tmp_file
                try:
                    rinterface.consolePrint('haha')
                except Exception as e:
                    sys.stdout = stdout
                    raise e
                sys.stdout = stdout
                tmp_file.flush()
                tmp_file.seek(0)
                self.assertEqual('haha', ''.join(s.decode() for s in tmp_file))

    def testCallErrorWhenEndedR(self):
        if sys.version_info[0] == 2 and sys.version_info[1] < 6:
            self.assertTrue(False) # cannot be tested with Python < 2.6
            return None
        import multiprocessing
        def foo(queue):
            import rpy2.rinterface as rinterface
            rdate = rinterface.baseenv['date']
            rinterface.endr(1)
            try:
                tmp = rdate()
                res = (False, None)
            except RuntimeError as re:
                res = (True, re)
            except Exception as e:
                res = (False, e)
            queue.put(res)
        q = multiprocessing.Queue()
        p = multiprocessing.Process(target = foo, args = (q,))
        p.start()
        res = q.get()
        p.join()
        self.assertTrue(res[0])

    def testStr_typeint(self):
        t = rinterface.baseenv['letters']
        self.assertEqual('STRSXP', rinterface.str_typeint(t.typeof))
        t = rinterface.baseenv['pi']
        self.assertEqual('REALSXP', rinterface.str_typeint(t.typeof))

    def testStr_typeint_invalid(self):
        self.assertRaises(LookupError, rinterface.str_typeint, 99)

    def testGet_initoptions(self):
        options = rinterface.get_initoptions()
        self.assertEqual(len(rinterface.initoptions),
                          len(options))
        for o1, o2 in zip(rinterface.initoptions, options):
            self.assertEqual(o1, o2)
        
    def testSet_initoptions(self):
        self.assertRaises(RuntimeError, rinterface.set_initoptions, 
                          ('aa', '--verbose', '--no-save'))

    def testInitr(self):
        def init_r(preserve_hash):
            from rpy2 import rinterface
            rinterface.initr(r_preservehash=preserve_hash)
        preserve_hash = True
        proc = multiprocessing.Process(target=init_r,
                                       args=(preserve_hash,))
        proc.start()
        proc.join()

    def testParse(self):
        xp = rinterface.parse("2 + 3")
        self.assertEqual(rinterface.EXPRSXP, xp.typeof)
        self.assertEqual(2.0, xp[0][1][0])
        self.assertEqual(3.0, xp[0][2][0])

    def testParseUnicode(self):
        xp = rinterface.parse(u'"\u21a7"')
        self.assertEqual(1, len(xp))
        self.assertEqual(1, len(xp[0]))

    def testParseIncompleteError(self):
        self.assertRaises(rinterface.RParsingIncompleteError,
                          rinterface.parse, "2 + 3 /")

    def testParseError(self):
        self.assertRaises(rinterface.RParsingError,
                          rinterface.parse, "2 + 3 , 1")

    def testRternalize(self):
        def f(x, y):
            return x[0]+y[0]
        rfun = rinterface.rternalize(f)
        res = rfun(1, 2)
        self.assertEqual(3, res[0])

    def testRternalizeNamedArgs(self):
        def f(x, y, z=None):
            if z is None:
                return x[0]+y[0]
            else:
                return z
        rfun = rinterface.rternalize(f)
        res = rfun(1, 2)
        self.assertEqual(3, res[0])
        res = rfun(1, 2, z=8)
        self.assertEqual(8, res[0])

    def testExternalPython(self):
        def f(x):
            return 3

        rpy_fun = rinterface.SexpExtPtr(f, tag = rinterface.python_type_tag)
        _python = rinterface.StrSexpVector(('.Python', ))
        res = rinterface.baseenv['.External'](_python,
                                              rpy_fun, 1)
        self.assertEqual(3, res[0])
        self.assertEqual(1, len(res))

    def testExternalPythonFromExpression(self):
        xp_name = rinterface.StrSexpVector(('expression',))
        xp = rinterface.baseenv['vector'](xp_name, 3)
        
        

    def testParseInvalidString(self):
        self.assertRaises(ValueError, rinterface.parse, 3)

    def testInterruptR(self):
        if sys.version_info[0] == 2 and sys.version_info[1] < 6:
            self.assertTrue(False) # Test unit currently requires Python >= 2.6
        with tempfile.NamedTemporaryFile(mode = 'w', suffix = '.py',
                                         delete = False) as rpy_code:
            rpy2_path = os.path.dirname(rpy2.__path__[0])

            rpy_code_str = """
import sys
sys.path.insert(0, '%s')
import rpy2.rinterface as ri

ri.initr()
def f(x):
    pass
ri.set_writeconsole_regular(f)
rcode = \"\"\"
i <- 0;
while(TRUE) {
    i <- i+1;
    Sys.sleep(0.01);
}\"\"\"
try:
  ri.baseenv['eval'](ri.parse(rcode))
except Exception as e:
  sys.exit(0)
      """ %(rpy2_path)

            rpy_code.write(rpy_code_str)
        cmd = (sys.executable, rpy_code.name)
        with open(os.devnull, 'w') as fnull:
            child_proc = subprocess.Popen(cmd,
                                          stdout=fnull,
                                          stderr=fnull)
            time.sleep(1)  # required for the SIGINT to function
            # (appears like a bug w/ subprocess)
            # (the exact sleep time migth be machine dependent :( )
            child_proc.send_signal(signal.SIGINT)
            time.sleep(1)  # required for the SIGINT to function
            ret_code = child_proc.poll()
        self.assertFalse(ret_code is None) # Interruption failed

    def testRpyMemory(self):
        x = rinterface.SexpVector(range(10), rinterface.INTSXP)
        y = rinterface.SexpVector(range(10), rinterface.INTSXP)
        x_rid = x.rid
        self.assertTrue(x_rid in set(z[0] for z in rinterface.protected_rids()))
        del(x)
        gc.collect(); gc.collect()
        self.assertFalse(x_rid in set(z[0] for z in rinterface.protected_rids()))

class CallbacksTestCase(unittest.TestCase):
    def tearDown(self):
        rinterface.set_writeconsole_regular(rinterface.consolePrint)
        rinterface.set_readconsole(rinterface.consoleRead)
        rinterface.set_readconsole(rinterface.consoleFlush)
        rinterface.set_choosefile(rinterface.chooseFile)
        sys.last_value = None

    def testSetWriteConsoleRegular(self):
        buf = []
        def f(x):
            buf.append(x)

        rinterface.set_writeconsole_regular(f)
        self.assertEqual(rinterface.get_writeconsole_regular(), f)
        code = rinterface.SexpVector(["3", ], rinterface.STRSXP)
        rinterface.baseenv["print"](code)
        self.assertEqual('[1] "3"\n', str.join('', buf))

    def testWriteConsoleRegularWithError(self):
        def f(x):
            raise CustomException("Doesn't work.")
        rinterface.set_writeconsole_regular(f)

        with tempfile.NamedTemporaryFile() as tmp_file:
            stderr = sys.stderr
            sys.stderr = tmp_file
            try:
                code = rinterface.SexpVector(["3", ], rinterface.STRSXP)
                rinterface.baseenv["print"](code)
            except Exception as e:
                sys.stderr = stderr
                raise e
            sys.stderr = stderr
            tmp_file.flush()
            tmp_file.seek(0)
            self.assertEqual("Doesn't work.", str(sys.last_value))
            #errorstring = ''.join(tmp_file.readlines())
            #self.assertTrue(errorstring.startswith('Traceback'))

    def testSetResetConsole(self):
        reset = [0]
        def f():
            reset[0] += 1

        rinterface.set_resetconsole(f)
        self.assertEqual(rinterface.get_resetconsole(), f)
        try:
            rinterface.baseenv['eval'](rinterface.parse('1+"a"'))
        except rinterface.RRuntimeError:
            pass
        self.assertEqual(1, reset[0])

    @onlyAQUAorWindows
    def testSetFlushConsole(self):
        flush = {'count': 0}
        def f():
            flush['count'] = flush['count'] + 1

        rinterface.set_flushconsole(f)
        self.assertEqual(rinterface.get_flushconsole(), f)
        rinterface.baseenv.get("flush.console")()
        self.assertEqual(1, flush['count'])
        rinterface.set_writeconsole_regular(rinterface.consoleFlush)

    @onlyAQUAorWindows
    def testFlushConsoleWithError(self):
        def f(prompt):
            raise Exception("Doesn't work.")
        rinterface.set_flushconsole(f)

        with tempfile.NamedTemporaryFile() as tmp_file:
            stderr = sys.stderr
            sys.stderr = tmp_file
            try:
                res = rinterface.baseenv.get("flush.console")()
            except Exception as e:
                sys.stderr = stderr
                raise e
            sys.stderr = stderr
            tmp_file.flush()
            tmp_file.seek(0)
            self.assertEqual("Doesn't work.", str(sys.last_value))
            #errorstring = ''.join(tmp_file.readlines())
            #self.assertTrue(errorstring.startswith('Traceback'))

    def testSetReadConsole(self):
        yes = "yes\n"
        def sayyes(prompt):
            return yes
        rinterface.set_readconsole(sayyes)
        self.assertEqual(rinterface.get_readconsole(), sayyes)
        res = rinterface.baseenv["readline"]()
        self.assertEqual(yes.strip(), res[0])
        rinterface.set_readconsole(rinterface.consoleRead)

    def testReadConsoleWithError(self):
        def f(prompt):
            raise Exception("Doesn't work.")
        rinterface.set_readconsole(f)

        with tempfile.NamedTemporaryFile() as tmp_file:

            stderr = sys.stderr
            sys.stderr = tmp_file
            try:
                res = rinterface.baseenv["readline"]()
            except Exception as e:
                sys.stderr = stderr
                raise e
            sys.stderr = stderr
            tmp_file.flush()
            tmp_file.seek(0)
            self.assertEqual("Doesn't work.", str(sys.last_value))
            #errorstring = ''.join(tmp_file.readlines())
            #self.assertTrue(errorstring.startswith('Traceback'))
        
    def testSetShowMessage(self):
        def f(message):
            return "foo"
        rinterface.set_showmessage(f)
        #FIXME: incomplete test

    def testShowMessageWithError(self):
        def f(prompt):
            raise Exception("Doesn't work.")
        rinterface.set_showmessage(f)
        #FIXME: incomplete test

    def testSetChooseFile(self):
        me = "me"
        def chooseMe(prompt):
            return me
        rinterface.set_choosefile(chooseMe)
        self.assertEqual(rinterface.get_choosefile(), chooseMe)
        res = rinterface.baseenv["file.choose"]()
        self.assertEqual(me, res[0])
        rinterface.set_choosefile(rinterface.chooseFile)

    def testChooseFileWithError(self):
        def noconsole(x):
            pass
        rinterface.set_writeconsole_regular(noconsole) # reverted by the tearDown method
        def f(prompt):
            raise Exception("Doesn't work.")
        rinterface.set_choosefile(f)
        self.assertRaises(rinterface.RRuntimeError,
                          rinterface.baseenv["file.choose"])
        self.assertEqual("Doesn't work.", str(sys.last_value))

    def testSetShowFiles(self):
        sf = []
        def f(fileheaders, wtitle, fdel, pager):
            sf.append(wtitle)
            for tf in fileheaders:
                sf.append(tf)

        rinterface.set_showfiles(f)
        file_path = rinterface.baseenv["file.path"]
        r_home = rinterface.baseenv["R.home"]
        filename = file_path(r_home(rinterface.StrSexpVector(("doc", ))), 
                             rinterface.StrSexpVector(("COPYRIGHTS", )))
        res = rinterface.baseenv["file.show"](filename)
        self.assertEqual(filename[0], sf[1][1])
        self.assertEqual('R Information', sf[0])

    def testShowFilesWithError(self):
        def f(fileheaders, wtitle, fdel, pager):
            raise Exception("Doesn't work.")

        rinterface.set_showfiles(f)
        file_path = rinterface.baseenv["file.path"]
        r_home = rinterface.baseenv["R.home"]
        filename = file_path(r_home(rinterface.StrSexpVector(("doc", ))), 
                             rinterface.StrSexpVector(("COPYRIGHTS", )))

        with tempfile.NamedTemporaryFile() as tmp_file:
            stderr = sys.stderr
            sys.stderr = tmp_file
            try:
                res = rinterface.baseenv["file.show"](filename)
            except rinterface.RRuntimeError:
                pass
            except Exception as e:
                sys.stderr = stderr
                raise e
            sys.stderr = stderr
            tmp_file.flush()
            tmp_file.seek(0)
            self.assertEqual("Doesn't work.", str(sys.last_value))
            #errorstring = ''.join(tmp_file.readlines())
            #self.assertTrue(errorstring.startswith('Traceback'))

    def testSetCleanUp(self):
        orig_cleanup = rinterface.get_cleanup()
        def f(saveact, status, runlast):
            return False
        rinterface.set_cleanup(f)
        rinterface.set_cleanup(orig_cleanup)

    def testCleanUp(self):
        orig_cleanup = rinterface.get_cleanup()
        def f(saveact, status, runlast):
            return None
        r_quit = rinterface.baseenv['q']
        rinterface.set_cleanup(f)        
        self.assertRaises(rinterface.RRuntimeError, r_quit)
        rinterface.set_cleanup(orig_cleanup)


class ObjectDispatchTestCase(unittest.TestCase):
    def testObjectDispatchLang(self):
        formula = rinterface.globalenv.get('formula')
        obj = formula(rinterface.StrSexpVector(['y ~ x', ]))
        self.assertTrue(isinstance(obj, rinterface.SexpVector))
        self.assertEqual(rinterface.LANGSXP, obj.typeof)

    def testObjectDispatchVector(self):
        letters = rinterface.globalenv.get('letters')
        self.assertTrue(isinstance(letters, rinterface.SexpVector))

    def testObjectDispatchClosure(self):
        #import pdb; pdb.set_trace()
        help = rinterface.globalenv.get('sum')
        self.assertTrue(isinstance(help, rinterface.SexpClosure))

    def testObjectDispatchRawVector(self):
        raw = rinterface.baseenv.get('raw')
        #rawvec = raw(rinterface.IntSexpVector((10, )))
        #self.assertEqual(rinterface.RAWSXP, rawvec.typeof)


class SerializeTestCase(unittest.TestCase):
    
    def testUnserialize(self):
        x = rinterface.IntSexpVector([1,2,3])
        x_serialized = x.__getstate__()
        x_again = rinterface.unserialize(x_serialized, x.typeof)
        identical = rinterface.baseenv["identical"]
        self.assertFalse(x.rsame(x_again))
        self.assertTrue(identical(x, x_again)[0])

    def testPickle(self):
        x = rinterface.IntSexpVector([1,2,3])
        with tempfile.NamedTemporaryFile() as f:
            pickle.dump(x, f)
            f.flush()
            f.seek(0)
            x_again = pickle.load(f)
        identical = rinterface.baseenv["identical"]
        self.assertTrue(identical(x, x_again)[0])


def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(EmbeddedRTestCase)
    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(CallbacksTestCase))
    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(ObjectDispatchTestCase))
    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(SerializeTestCase))
    return suite

if __name__ == '__main__':
    tr = unittest.TextTestRunner(verbosity = 2)
    tr.run(suite())
    
