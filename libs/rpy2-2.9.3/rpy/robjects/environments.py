import rpy2.rinterface as rinterface
from rpy2.robjects.robject import RObjectMixin, RObject
from rpy2.robjects import conversion

_new_env = rinterface.baseenv["new.env"]

class Environment(RObjectMixin, rinterface.SexpEnvironment):
    """ An R environement, implementing Python's mapping interface. """
    
    def __init__(self, o=None):
        if o is None:
            o = _new_env(hash=rinterface.SexpVector([True, ], 
                                                    rinterface.LGLSXP))
        super(Environment, self).__init__(o)

    def __getitem__(self, item):
        res = super(Environment, self).__getitem__(item)
        res = conversion.converter.ri2ro(res)
        # objects in a R environment have an associated name / symbol
        try:
            res.__rname__ = item
        except AttributeError:
            # the 3rd-party conversion function can return objects
            # for which __rname__ cannot be set (because of fixed
            # __slots__ and no __rname__ in the original set
            # of attributes)
            pass
        return res

    def __setitem__(self, item, value):
        robj = conversion.converter.py2ri(value)
        super(Environment, self).__setitem__(item, robj)

    def get(self, item, wantfun = False):
        """ Get a object from its R name/symol
        :param item: string (name/symbol)
        :rtype: object (as returned by :func:`conversion.converter.ri2ro`)
        """
        res = super(Environment, self).get(item, wantfun = wantfun)
        res = conversion.converter.ri2ro(res)
        res.__rname__ = item
        return res

    def keys(self):
        """ Return a tuple listing the keys in the object """
        return (x for x in self)

    def items(self):
        """ Iterate through the symbols and associated objects in
            this R environment."""
        for k in self:
            yield (k, self[k])

    def values(self):
        """ Iterate through the objects in
            this R environment."""
        for k in self:
            yield self[k]

    def pop(self, *args):
        """ E.pop(k[, d]) -> v, remove the specified key
        and return the corresponding value. If the key is not found,
        d is returned if given, otherwise KeyError is raised."""
        l = len(args)
        k = args[0]
        if k in self:
            v = self[k]
            del(self[k])
        elif l == 1:
            raise KeyError(k)
        else:
            v = args[1]
        return v

    def popitem(self):
        """ E.popitem() -> (k, v), remove and return some (key, value)
        pair as a 2-tuple; but raise KeyError if E is empty. """
        if len(self) == 0:
            raise KeyError()
        kv = next(self.items())
        del(self[kv[0]])
        return kv

    def clear(self):
        """ E.clear() -> None.  Remove all items from D. """
        ## FIXME: is there a more efficient implementation (when large
        ##        number of keys) ?
        for k in self:
            del(self[k])
