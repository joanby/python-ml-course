import unittest
import rpy2.robjects as robjects
rinterface = robjects.rinterface
import rpy2.rlike.container as rlc

import array
import csv, tempfile

class DataFrameTestCase(unittest.TestCase):

    def testNewFromTaggedList(self):
        letters = robjects.r.letters        
        numbers = robjects.r('1:26')
        df = robjects.DataFrame(rlc.TaggedList((letters, numbers),
                                               tags = ('letters', 'numbers')))

        self.assertEqual("data.frame", df.rclass[0])

    def testNewFromRObject(self):
        numbers = robjects.r('1:5')
        self.assertRaises(ValueError, robjects.DataFrame, numbers)

        rfunc = robjects.r('sum')
        self.assertRaises(ValueError, robjects.DataFrame, rfunc)

        rdataf = robjects.r('data.frame(a=1:2, b=c("a", "b"))')
        dataf = robjects.DataFrame(rdataf)        

    def testNewFromOrdDict(self):
        od = rlc.OrdDict(c=(('a', robjects.IntVector((1,2))),
                            ('b', robjects.StrVector(('c', 'd')))
                            ))
        dataf = robjects.DataFrame(od)
        self.assertEqual(1, dataf.rx2('a')[0])

    def testNewFromDict(self):
        od = {'a': robjects.IntVector((1,2)),
              'b': robjects.StrVector(('c', 'd'))}
        dataf = robjects.DataFrame(od)
        self.assertEqual(1, dataf.rx2('a')[0])

    def testNewStringsAsFactors(self):
        od = {'a': robjects.IntVector((1,2)),
              'b': robjects.StrVector(('c', 'd'))}
        dataf = robjects.DataFrame(od, stringsasfactor=True)
        self.assertTrue(isinstance(dataf.rx2('b'), robjects.FactorVector))
        dataf = robjects.DataFrame(od, stringsasfactor=False)
        self.assertTrue(isinstance(dataf.rx2('b'), robjects.StrVector))

    def testDim(self):
        letters = robjects.r.letters        
        numbers = robjects.r('1:26')
        df = robjects.DataFrame(rlc.TaggedList((letters, numbers),
                                               tags = ('letters', 'numbers')))
        self.assertEqual(26, df.nrow)
        self.assertEqual(2, df.ncol)

    def testFrom_csvfile(self):
        column_names = ('letter', 'value')
        data = (column_names,
                ('a', 1),
                ('b', 2),
                ('c', 3))
        fh = tempfile.NamedTemporaryFile(mode = "w", delete = False)
        csv_w = csv.writer(fh)
        csv_w.writerows(data)
        fh.close()
        dataf = robjects.DataFrame.from_csvfile(fh.name)
        self.assertEqual(column_names, tuple(dataf.names))
        self.assertEqual(3, dataf.nrow)
        self.assertEqual(2, dataf.ncol)

    def testTo_csvfile(self):
        fh = tempfile.NamedTemporaryFile(mode = "w", delete = False)
        fh.close()
        d = {'letter': robjects.StrVector('abc'),
             'value' : robjects.IntVector((1, 2, 3))}
        dataf = robjects.DataFrame(d)
        dataf.to_csvfile(fh.name)
        dataf = robjects.DataFrame.from_csvfile(fh.name)
        self.assertEqual(3, dataf.nrow)
        self.assertEqual(2, dataf.ncol)

    def testIter_col(self):
        dataf = robjects.r('data.frame(a=1:2, b=I(c("a", "b")))')
        col_types = [x.typeof for x in dataf.iter_column()]
        self.assertEqual(rinterface.INTSXP, col_types[0])
        self.assertEqual(rinterface.STRSXP, col_types[1])

    def testIter_row(self):
        dataf = robjects.r('data.frame(a=1:2, b=I(c("a", "b")))')
        rows = [x for x in dataf.iter_row()]
        self.assertEqual(1, rows[0][0][0])
        self.assertEqual("b", rows[1][1][0])

    def testColnames(self):
        dataf = robjects.r('data.frame(a=1:2, b=I(c("a", "b")))')
        self.assertEqual('1', dataf.rownames[0])
        self.assertEqual('2', dataf.rownames[1])

    def testColnames_set(self):
        dataf = robjects.r('data.frame(a=1:2, b=I(c("a", "b")))')
        dataf.colnames = robjects.StrVector('de')
        self.assertEqual('d', dataf.colnames[0])
        self.assertEqual('e', dataf.colnames[1])

    def testRownames(self):
        dataf = robjects.r('data.frame(a=1:2, b=I(c("a", "b")))')
        self.assertEqual('a', dataf.colnames[0])
        self.assertEqual('b', dataf.colnames[1])        

    def testRownames_set(self):
        dataf = robjects.r('data.frame(a=1:2, b=I(c("a", "b")))')
        dataf.rownames = robjects.StrVector('de')
        self.assertEqual('d', dataf.rownames[0])
        self.assertEqual('e', dataf.rownames[1])

    def testCbind(self):
        dataf = robjects.r('data.frame(a=1:2, b=I(c("a", "b")))')
        dataf = dataf.cbind(robjects.r('data.frame(a=1:2, b=I(c("a", "b")))'))
        self.assertEqual(4, dataf.ncol)
        self.assertEqual(2, len([x for x in dataf.colnames if x == 'a']))

        dataf = robjects.r('data.frame(a=1:2, b=I(c("a", "b")))')
        dataf = dataf.cbind(a = robjects.StrVector(("c", "d")))
        self.assertEqual(3, dataf.ncol)
        self.assertEqual(2, len([x for x in dataf.colnames if x == 'a']))

def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(DataFrameTestCase)
    return suite

if __name__ == '__main__':
     unittest.main()
