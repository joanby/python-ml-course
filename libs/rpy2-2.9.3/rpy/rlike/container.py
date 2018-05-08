import sys
if sys.version_info[0] == 2:
    range = xrange
    from itertools import izip as zip
import rpy2.rlike.indexing as rli

class OrdDict(dict):
    """ Implements the Ordered Dict API defined in PEP 372.
    When `odict` becomes part of collections, this class 
    should inherit from it rather than from `dict`.

    This class differs a little from the Ordered Dict
    proposed in PEP 372 by the fact that:
    not all elements have to be named. None as a key value means
    an absence of name for the element.

    """

    def __init__(self, c=[]):

        if isinstance(c, TaggedList) or isinstance(c, OrdDict):
            c = c.items()
        elif isinstance(c, dict):
            #FIXME: allow instance from OrdDict ?
            raise ValueError('A regular dictionnary does not ' +\
                             'conserve the order of its keys.')

        super(OrdDict, self).__init__()
        self.__l = []

        for k,v in c:
            self[k] = v

    def __copy__(self):
        cp = OrdDict(c = tuple(self.items()))
        return cp
        
    def __cmp__(self, o):
        raise(Exception("Not yet implemented."))

    def __eq__(self):
        raise(Exception("Not yet implemented."))
        
    def __getitem__(self, key):
        if key is None:
            raise ValueError("Unnamed items cannot be retrieved by key.")
        i = super(OrdDict, self).__getitem__(key)
        return self.__l[i][1]

    def __iter__(self):
        l = self.__l
        for e in l:
            k = e[0]
            if k is None:
                continue
            else:
                yield k

    def __len__(self):
        return len(self.__l)

    def __ne__(self):
        raise(Exception("Not yet implemented."))

    def __repr__(self):
        s = 'o{'
        for k,v in self.items():
            s += "'" + str(k) + "': " + str(v) + ", "
        s += '}'
        return s

    def __reversed__(self):
        raise(Exception("Not yet implemented."))

    def __setitem__(self, key, value):
        """ Replace the element if the key is known, 
        and conserve its rank in the list, or append
        it if unknown. """

        if key is None:
            self.__l.append((key, value))
            return

        if key in self:
            i = self.index(key)
            self.__l[i] = (key, value)
        else:
            self.__l.append((key, value))
            super(OrdDict, self).__setitem__(key, len(self.__l)-1)
            
    def byindex(self, i):
        """ Fetch a value by index (rank), rather than by key."""
        return self.__l[i]

    def index(self, k):
        """ Return the index (rank) for the key 'k' """
        return super(OrdDict, self).__getitem__(k)

    def get(self, k, d = None):
        """ OD.get(k[,d]) -> OD[k] if k in OD, else d.  d defaults to None """
        try:
            res = self[k]
        except KeyError as ke:
            res = d
        return res

    def items(self):
        """ OD.items() -> an iterator over the (key, value) items of D """
        return iter(self.__l)

    def keys(self):
        """ """
        return tuple([x[0] for x in self.__l])

    def reverse(self):
        """ Reverse the order of the elements in-place (no copy)."""
        l = self.__l
        n = len(self.__l)
        for i in range(n//2):
            tmp = l[i]
            l[i] = l[n-i-1]
            kv = l[i]
            if kv is not None:
                super(OrdDict, self).__setitem__(kv[0], i)
            l[n-i-1] = tmp
            kv = tmp
            if kv is not None:
                super(OrdDict, self).__setitem__(kv[0], n-i-1)
            

    def sort(self, cmp=None, key=None, reverse=False):
        raise(Exception("Not yet implemented."))

    
class TaggedList(list):
    """ A list for which each item has a 'tag'. 

    :param l: list
    :param tag: optional sequence of tags

    """

    def __add__(self, tl):
        try:
            tags = tl.tags
        except AttributeError as ae:
            raise ValueError('Can only concatenate TaggedLists.')
        res = TaggedList(list(self) + list(tl),
                         tags = self.tags + tl.tags)
        return res

    def __delitem__(self, y):
        super(TaggedList, self).__delitem__(y)
        self.__tags.__delitem__(y)

    def __delslice__(self, i, j):
        super(TaggedList, self).__delslice__(i, j)
        self.__tags.__delslice__(i, j)

    def __iadd__(self, y):
        super(TaggedList, self).__iadd__(y)
        if isinstance(y, TaggedList):
            self.__tags.__iadd__(y.tags)
        else:
            self.__tags.__iadd__([None, ] * len(y))
        return self

    def __imul__(self, y):
        restags = self.__tags.__imul__(y)
        resitems = super(TaggedList, self).__imul__(y)
        return self

    @staticmethod
    def from_items(tagval):
        res = TaggedList([])
        for k,v in tagval.items():
            res.append(v, tag=k)
        return res
    
    def __init__(self, seq, tags = None):
        super(TaggedList, self).__init__(seq)
        if tags is None:
            tags = [None, ] * len(seq)
        if len(tags) != len(seq):
            raise ValueError("There must be as many tags as seq")
        self.__tags = list(tags)

    def __setslice__(self, i, j, y):
        super(TaggedList, self).__setslice__(i, j, y)
        #FIXME: handle TaggedList ?
        #self.__tags.__setslice__(i, j, [None, ])

    def append(self, obj, tag = None):
        """ Append an object to the list
        :param obj: object
        :param tag: object
        """
        super(TaggedList, self).append(obj)
        self.__tags.append(tag)

    def extend(self, iterable):
        """ Extend the list with an iterable object.

        :param iterable: iterable object
        """

        if isinstance(iterable, TaggedList):
            itertags = iterable.itertags()
        else:
            itertags = [None, ] * len(iterable)

        for tag, item in zip(itertags, iterable):
            self.append(item, tag=tag)


    def insert(self, index, obj, tag=None):
        """
        Insert an object in the list

        :param index: integer
        :param obj: object
        :param tag: object

        """
        super(TaggedList, self).insert(index, obj)
        self.__tags.insert(index, tag)

    def items(self):
        """
        Return a tuple of all pairs (tag, item).

        :rtype: tuple of 2-element tuples (tag, item)
        """

        res = [(tag, item) for tag, item in zip(self.__tags, self)]
        return tuple(res)

    def iterontag(self, tag):
        """
        iterate on items marked with one given tag.
        
        :param tag: object
        """

        i = 0
        for onetag in self.__tags:
            if tag == onetag:
                yield self[i]
            i += 1

    def items(self):
        """ OD.items() -> an iterator over the (key, value) items of D """
        for tag, item in zip(self.__tags, self):
            yield (tag, item)
        
    def itertags(self):
        """
        iterate on tags.
        
        :rtype: iterator
        """
        for tag in self.__tags:
            yield tag

    def pop(self, index=None):
        """
        Pop the item at a given index out of the list

        :param index: integer
        
        """
        if index is None:
            index = len(self) - 1
        
        res = super(TaggedList, self).pop(index)
        self.__tags.pop(index)
        return res

    def remove(self, value):
        """ 
        Remove a given value from the list.
        
        :param value: object
        
        """
        found = False
        for i in range(len(self)):
            if self[i] == value:
                found = True
                break
        if found:
            self.pop(i)

    def reverse(self):
        """ Reverse the order of the elements in the list. """
        super(TaggedList, self).reverse()
        self.__tags.reverse()

    def sort(self, reverse = False):
        """ 
        Sort in place
        """
        o = rli.order(self, reverse = reverse)
        super(TaggedList, self).sort(reverse = reverse)
        self.__tags = [self.__tags[i] for i in o]

    def __get_tags(self):
        return tuple(self.__tags)

    def __set_tags(self, tags):
        if len(tags) == len(self.__tags):
            self.__tags = tuple(tags)
        else:
            raise ValueError("The new list of tags should have the same length as the old one")
    tags = property(__get_tags, __set_tags)
        
    def settag(self, i, t):
        """
        Set tag 't' for item 'i'.
        
        :param i: integer (index)

        :param t: object (tag)
        """
        self.__tags[i] = t


# class DataFrame(ArgsDict):

#     def __init__(self, s):
#         super(ArgsDict, self).__init__(s)
        
#         if len(self) > 0:
#             nrows = len(self[0])
#             for i, v in enumerate(self):
#                 if len(v) != nrows:
#                     raise ValueError("Expected length %i for element %i" 
#                                      %(nrows, i))
                
