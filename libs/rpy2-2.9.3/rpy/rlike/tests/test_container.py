import unittest
import sys
if sys.version_info[0] == 2:
    from itertools import izip as zip
import rpy2.rlike.container as rlc

class OrdDictTestCase(unittest.TestCase):

    def testNew(self):
        nl = rlc.OrdDict()

        x = (('a', 123), ('b', 456), ('c', 789))
        nl = rlc.OrdDict(x)

    def testLen(self):
        x = rlc.OrdDict()
        self.assertEqual(0, len(x))

        x['a'] = 2
        x['b'] = 1

        self.assertEqual(2, len(x))

    def testGetSetitem(self):
        x = rlc.OrdDict()
        
        x['a'] = 1
        self.assertEqual(1, len(x))
        self.assertEqual(1, x['a'])
        self.assertEqual(0, x.index('a'))
        x['a'] = 2
        self.assertEqual(1, len(x))
        self.assertEqual(2, x['a'])
        self.assertEqual(0, x.index('a'))
        x['b'] = 1
        self.assertEqual(2, len(x))
        self.assertEqual(1, x['b'])
        self.assertEqual(1, x.index('b'))

    def testGet(self):
        x = rlc.OrdDict()
        x['a'] = 1
        self.assertEqual(1, x.get('a'))
        self.assertEqual(None, x.get('b'))
        self.assertEqual(2, x.get('b', 2))
        
    def testKeys(self):
        x = rlc.OrdDict()
        for i,k in enumerate('abcdef'):
            x[k] = i
        for i,k in enumerate(x.keys()):
            self.assertEqual('abcdef'[i], k)

    def testGetSetitemWithNone(self):
        x = rlc.OrdDict()
        
        x['a'] = 1
        x[None] = 2
        self.assertEqual(2, len(x))
        x['b'] = 5
        self.assertEqual(3, len(x))
        self.assertEqual(1, x['a'])
        self.assertEqual(5, x['b'])
        self.assertEqual(0, x.index('a'))
        self.assertEqual(2, x.index('b'))
        
    def testReverse(self):
        x = rlc.OrdDict()
        x['a'] = 3
        x['b'] = 2
        x['c'] = 1
        x.reverse()
        self.assertEqual(1, x['c'])
        self.assertEqual(0, x.index('c'))
        self.assertEqual(2, x['b'])
        self.assertEqual(1, x.index('b'))
        self.assertEqual(3, x['a'])
        self.assertEqual(2, x.index('a'))

    def testItems(self):
        args = (('a', 5), ('b', 4), ('c', 3),
                ('d', 2), ('e', 1))
        x = rlc.OrdDict(args)
        it = x.items()
        for ki, ko in zip(args, it):
            self.assertEqual(ki[0], ko[0])
            self.assertEqual(ki[1], ko[1])

class TaggedListTestCase(unittest.TestCase):

    def test__add__(self):
        tl = rlc.TaggedList((1,2,3), tags=('a', 'b', 'c'))        
        tl = tl + tl
        self.assertEqual(6, len(tl))
        self.assertEqual(('a', 'b', 'c', 'a', 'b', 'c'), tl.tags)
        self.assertEqual((1,2,3,1,2,3), tuple(tl))

    def test__delitem__(self):
        tl = rlc.TaggedList((1,2,3), tags=('a', 'b', 'c'))        
        self.assertEqual(3, len(tl))
        del tl[1]
        self.assertEqual(2, len(tl))
        self.assertEqual(tl.tags, ('a', 'c'))
        self.assertEqual(tuple(tl), (1, 3))

    def test__delslice__(self):
        tl = rlc.TaggedList((1,2,3,4), tags=('a', 'b', 'c', 'd'))        
        del tl[1:3]
        self.assertEqual(2, len(tl))
        self.assertEqual(tl.tags, ('a', 'd'))
        self.assertEqual(tuple(tl), (1, 4))

    def test__iadd__(self):
        tl = rlc.TaggedList((1,2,3), tags=('a', 'b', 'c'))        
        tl += tl
        self.assertEqual(6, len(tl))
        self.assertEqual(('a', 'b', 'c', 'a', 'b', 'c'), tl.tags)
        self.assertEqual((1,2,3,1,2,3), tuple(tl))

    def test__imul__(self):
        tl = rlc.TaggedList((1,2), tags=('a', 'b'))
        tl *= 3
        self.assertEqual(6, len(tl))
        self.assertEqual(('a', 'b', 'a', 'b', 'a', 'b'), tl.tags)
        self.assertEqual((1,2,1,2,1,2), tuple(tl))

    def test__init__(self):
        tl = rlc.TaggedList((1,2,3), tags=('a', 'b', 'c'))        
        self.assertRaises(ValueError, rlc.TaggedList, (1,2,3), tags = ('b', 'c'))

    def test__setslice__(self):
        tl = rlc.TaggedList((1,2,3,4), tags=('a', 'b', 'c', 'd'))        
        tl[1:3] = [5, 6]
        self.assertEqual(4, len(tl))
        self.assertEqual(tl.tags, ('a', 'b', 'c', 'd'))
        self.assertEqual(tuple(tl), (1, 5, 6, 4))

    def testappend(self):
        tl = rlc.TaggedList((1,2,3), tags=('a', 'b', 'c'))        
        self.assertEqual(3, len(tl))
        tl.append(4, tag='a')
        self.assertEqual(4, len(tl))
        self.assertEqual(4, tl[3])
        self.assertEqual(('a', 'b', 'c', 'a'), tl.tags)

    def testextend(self):
        tl = rlc.TaggedList((1,2,3), tags=('a', 'b', 'c'))        
        tl.extend([4, 5])
        self.assertEqual(('a', 'b', 'c', None, None), tuple(tl.itertags()))
        self.assertEqual((1, 2, 3, 4, 5), tuple(tl))

    def testinsert(self):
        tl = rlc.TaggedList((1,2,3), tags=('a', 'b', 'c'))        
        tl.insert(1, 4, tag = 'd')
        self.assertEqual(('a', 'd', 'b', 'c'), tuple(tl.itertags()))
        self.assertEqual((1, 4, 2, 3), tuple(tl))
        
    def testitems(self):
        tl = rlc.TaggedList((1,2,3), tags=('a', 'b', 'c'))        
        self.assertEqual((('a', 1), ('b', 2), ('c', 3)), 
                          tuple(tl.items()))

    def testiterontag(self):
        tl = rlc.TaggedList((1,2,3), tags=('a', 'b', 'a'))
        self.assertEqual((1, 3), tuple(tl.iterontag('a')))

    def testitertags(self):
        tl = rlc.TaggedList((1,2,3), tags=('a', 'b', 'c'))
        self.assertEqual(('a', 'b', 'c'), tuple(tl.itertags()))

    def testpop(self):
        tl = rlc.TaggedList((1,2,3), tags=('a', 'b', 'c'))
        self.assertEqual(3, len(tl))
        elt = tl.pop()
        self.assertEqual(3, elt)
        self.assertEqual(2, len(tl))
        self.assertEqual(tl.tags, ('a', 'b'))
        self.assertEqual(tuple(tl), (1, 2))

        elt = tl.pop(0)
        self.assertEqual(1, elt)
        self.assertEqual(1, len(tl))
        self.assertEqual(tl.tags, ('b', ))

    def testremove(self):
        tl = rlc.TaggedList((1,2,3), tags=('a', 'b', 'c'))
        self.assertEqual(3, len(tl))
        tl.remove(2)
        self.assertEqual(2, len(tl))
        self.assertEqual(tl.tags, ('a', 'c'))
        self.assertEqual(tuple(tl), (1, 3))

    def testreverse(self):
        tn = ['a', 'b', 'c']
        tv = [1,2,3]
        tl = rlc.TaggedList(tv, tags = tn)
        tl.reverse()
        self.assertEqual(3, len(tl))
        self.assertEqual(tl.tags, ('c', 'b', 'a'))
        self.assertEqual(tuple(tl), (3, 2, 1))

    def testsort(self):
        tn = ['a', 'c', 'b']
        tv = [1,3,2]
        tl = rlc.TaggedList(tv, tags = tn)
        tl.sort()

        self.assertEqual(tl.tags, ('a', 'b', 'c'))
        self.assertEqual(tuple(tl), (1, 2, 3))
        
    def testtags(self):
        tn = ['a', 'b', 'c']
        tv = [1,2,3]
        tl = rlc.TaggedList(tv, tags = tn)
        tags = tl.tags
        self.assertTrue(isinstance(tags, tuple))
        self.assertEqual(tags, ('a', 'b', 'c'))

        tn = ['d', 'e', 'f']
        tl.tags = tn
        self.assertTrue(isinstance(tags, tuple))
        self.assertEqual(tuple(tn), tl.tags)

    def testsettag(self):
        tn = ['a', 'b', 'c']
        tv = [1,2,3]
        tl = rlc.TaggedList(tv, tags = tn)
        tl.settag(1, 'z')
        self.assertEqual(tl.tags, ('a', 'z', 'c'))

    def testfrom_items(self):
        od = rlc.OrdDict( (('a', 1), ('b', 2), ('c', 3)) )
        tl = rlc.TaggedList.from_items(od)
        self.assertEqual(('a', 'b', 'c'), tl.tags)
        self.assertEqual((1, 2, 3), tuple(tl))

        tl = rlc.TaggedList.from_items({'a':1, 'b':2, 'c':3})
        self.assertEqual(set(('a', 'b', 'c')), set(tl.tags))
        self.assertEqual(set((1, 2, 3)), set(tuple(tl)))
        
def suite():
    suite = unittest.TestLoader().loadTestsFromTestCase(OrdDictTestCase)
    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(TaggedListTestCase))
    return suite

if __name__ == '__main__':
     unittest.main()
