"""
R objects as Python objects.

The module is structured around the singleton r of class R,
that represents an embedded R.

License: GPLv2+

"""

import os, sys
import types
import array
import itertools
from datetime import datetime
import rpy2.rinterface as rinterface
import rpy2.rlike.container as rlc

from rpy2.robjects.robject import RObjectMixin, RObject
from rpy2.robjects.vectors import (BoolVector,
                                   IntVector,
                                   FloatVector,
                                   ComplexVector,
                                   StrVector,
                                   FactorVector,
                                   Vector,
                                   ListVector,
                                   DateVector,
                                   POSIXct,
                                   POSIXlt,
                                   Array,
                                   Matrix,
                                   DataFrame)
from rpy2.robjects.functions import Function, SignatureTranslatedFunction
from rpy2.robjects.environments import Environment
from rpy2.robjects.methods import RS4

from . import conversion

from rpy2.rinterface import (Sexp, 
                             SexpVector, 
                             SexpClosure, 
                             SexpEnvironment, 
                             SexpS4,
                             StrSexpVector,
                             SexpExtPtr)
_globalenv = rinterface.globalenv

# missing values
from rpy2.rinterface import (NA_Real, 
                             NA_Integer, 
                             NA_Logical, 
                             NA_Character, 
                             NA_Complex, 
                             NULL)

_rparse = rinterface.baseenv['parse']
_reval = rinterface.baseenv['eval']

def reval(string, envir = _globalenv):
    """ Evaluate a string as R code
    - string: a string
    - envir: an environment in which the environment should take place
             (default: R's global environment)
    """
    p = rinterface.parse(string)
    res = _reval(p, envir = envir)
    return res

default_converter = conversion.Converter('base empty converter')

@default_converter.ri2ro.register(RObject)
def _(obj):
    return obj


def sexpvector_to_ro(obj):
    try:
        rcls = obj.do_slot("class")
    except LookupError as le:
        rcls = [None]

    if 'data.frame' in rcls:
        res = vectors.DataFrame(obj)
        return res
    try:
        dim = obj.do_slot("dim")
        if len(dim) == 2:
            res = vectors.Matrix(obj)
        else:
            res = vectors.Array(obj)
    except LookupError as le:
        if obj.typeof == rinterface.INTSXP:
            if 'factor' in rcls:
                res = vectors.FactorVector(obj)
            else:
                res = vectors.IntVector(obj)
        elif obj.typeof == rinterface.REALSXP:
            if obj.rclass[0] == 'POSIXct':
                res = vectors.POSIXct(obj)
            else:
                res = vectors.FloatVector(obj)
        elif obj.typeof == rinterface.LGLSXP:
            res = vectors.BoolVector(obj)
        elif obj.typeof == rinterface.STRSXP:
            res = vectors.StrVector(obj)
        elif obj.typeof == rinterface.VECSXP:
            res = vectors.ListVector(obj)
        elif obj.typeof == rinterface.LANGSXP and 'formula' in rcls:
            res = Formula(obj)
        else:
            res = vectors.Vector(obj)
    return res

default_converter.ri2ro.register(SexpVector, sexpvector_to_ro)

TYPEORDER = {bool: (0, BoolVector),
             int: (1, IntVector),
             float: (2, FloatVector),
             complex: (3, ComplexVector),
             str: (4, StrVector)}
def sequence_to_vector(lst):
    curr_typeorder = -1
    i = None
    for i, elt in enumerate(lst):
        cls = type(elt)
        if cls in TYPEORDER:
            if TYPEORDER[cls][0] > curr_typeorder:
                curr_typeorder, curr_type = TYPEORDER[cls]
        else:
            raise ValueError('The element %i in the list has a type that cannot be handled.' % i)
    if i is None:
        raise ValueError('The parameter "lst" is an empty sequence. The type of the corresponding R vector cannot be determined.')
    res = curr_type(lst)
    return res


@default_converter.ri2ro.register(SexpClosure)
def _(obj):
    return SignatureTranslatedFunction(obj)

@default_converter.ri2ro.register(SexpEnvironment)
def _(obj):
    return Environment(obj)

@default_converter.ri2ro.register(SexpS4)
def _(obj):
    return RS4(obj)

@default_converter.ri2ro.register(SexpExtPtr)
def _(obj):
    return obj

@default_converter.ri2ro.register(object)
def _(obj):
    return RObject(obj)

@default_converter.ri2ro.register(type(NULL))
def _(obj):
    return obj


def default_py2ri(o):
    """ Convert an arbitrary Python object to a
    :class:`rpy2.rinterface.Sexp` object.
    Creates an R object with the content of the Python object,
    wich means data copying.
    :param o: object
    :rtype: :class:`rpy2.rinterface.Sexp` (and subclasses)
    """
    pass

@default_converter.py2ri.register(RObject)
def _(obj):
    return rinterface.Sexp(obj)

@default_converter.py2ri.register(Sexp)
def _(obj):
    return obj

@default_converter.py2ri.register(array.array)
def _(obj):
    if obj.typecode in ('h', 'H', 'i', 'I'):
        res = rinterface.SexpVector(obj, rinterface.INTSXP)
    elif obj.typecode in ('f', 'd'):
        res = rinterface.SexpVector(obj, rinterface.REALSXP)
    else:
        raise(ValueError("Nothing can be done for this array type at the moment."))
    return res

@default_converter.py2ri.register(bool)
def _(obj):
    return rinterface.SexpVector([obj, ], rinterface.LGLSXP)

def int2ri(obj):
    # special case for NA_Logical
    if obj is rinterface.NA_Logical:
        res = rinterface.SexpVector([obj, ], rinterface.LGLSXP)
    else:
        res = rinterface.SexpVector([obj, ], rinterface.INTSXP)
    return res

default_converter.py2ri.register(int, int2ri)

@default_converter.py2ri.register(float)
def _(obj):
    return rinterface.SexpVector([obj, ], rinterface.REALSXP)

@default_converter.py2ri.register(bytes)
def _(obj):
    return rinterface.SexpVector([obj, ], rinterface.STRSXP)

@default_converter.py2ri.register(str)
def _(obj):
    return rinterface.SexpVector([obj, ], rinterface.STRSXP)

@default_converter.py2ri.register(list)
def _(obj):
    return rinterface.ListSexpVector([conversion.py2ri(x) for x in obj])

@default_converter.py2ri.register(rlc.TaggedList)
def _(obj):
    res = rinterface.ListSexpVector([conversion.py2ri(x) for x in obj])
    res.do_slot_assign('names', rinterface.StrSexpVector(obj.tags))
    return res

@default_converter.py2ri.register(complex)
def _(obj):
    return rinterface.SexpVector([obj, ], rinterface.CPLXSXP)


@default_converter.py2ro.register(object)
def _(obj):
    robj = conversion.py2ri(obj)
    return conversion.ri2ro(robj)

@default_converter.py2ri.register(types.FunctionType)
def _function_to_ri(func):
    def wrap(*args):
        res = func(*args)
        res = conversion.py2ro(res)
        return res
    rfunc = rinterface.rternalize(wrap)
    return rfunc

@default_converter.ri2py.register(object)
def _(obj):
    return conversion.ri2ro(obj)

class Formula(RObjectMixin, rinterface.Sexp):

    def __init__(self, formula, environment = _globalenv):
        if isinstance(formula, str):
            inpackage = rinterface.baseenv["::"]
            asformula = inpackage(rinterface.StrSexpVector(['stats', ]),
                                  rinterface.StrSexpVector(['as.formula', ]))
            formula = rinterface.SexpVector(rinterface.StrSexpVector([formula, ]))
            robj = asformula(formula,
                             env = environment)
        else:
            robj = formula
        super(Formula, self).__init__(robj)

    def getenvironment(self):
        """ Get the environment in which the formula is finding its symbols."""
        res = self.do_slot(".Environment")
        res = conversion.ri2ro(res)
        return res

    def setenvironment(self, val):
        """ Set the environment in which a formula will find its symbols."""
        if not isinstance(val, rinterface.SexpEnvironment):
            raise ValueError("The environment must be an instance of" +
                             " rpy2.rinterface.Sexp.environment")
        self.do_slot_assign(".Environment", val)

    environment = property(getenvironment, setenvironment,
                           "R environment in which the formula will look for" +
                           " its variables.")

    
class ParsedCode(rinterface.SexpVector):
    def __init__(self, source=None):
        self._source = source
        super(ParsedCode, self).__init__(self)

    @property
    def source(self):
        return self._source

    
class SourceCode(str):
    
    def parse(self, keep_source=True):
        res = _rparse(text=rinterface.StrSexpVector((self,)))
        if keep_source:
            res = ParsedCode(res, source=self)
        else:
            res = ParsedCode(res, source=None)
        return res
    
    def as_namespace(self, name):
        """ Name for the namespace """
        return SignatureTranslatedAnonymousPackage(self,
                                                   name)

class R(object):
    """
    Singleton representing the embedded R running.
    """
    _instance = None

    def __new__(cls):
        if cls._instance is None:
            rinterface.initr()
            cls._instance = object.__new__(cls)
        return cls._instance

    def __getattribute__(self, attr):
        try:
            return super(R, self).__getattribute__(attr)
        except AttributeError as ae:
            orig_ae = str(ae)

        try:
            return self.__getitem__(attr)
        except LookupError as le:
            raise AttributeError(orig_ae)

    def __getitem__(self, item):
        res = _globalenv.get(item)
        res = conversion.ri2py(res)
        if hasattr(res, '__rname__'):
            res.__rname__ = item
        return res

    #FIXME: check that this is properly working
    def __cleanup__(self):
        rinterface.endEmbeddedR()
        del(self)

    def __str__(self):
        s = super(R, self).__str__()
        s += os.linesep
        version = self["version"]
        tmp = [n+': '+val[0] for n, val in zip(version.names, version)]
        s += str.join(os.linesep, tmp)
        return s

    def __call__(self, string):
        p = _rparse(text=StrSexpVector((string,)))
        res = self.eval(p)
        return conversion.ri2py(res)

r = R()

conversion.set_conversion(default_converter)

globalenv = conversion.converter.ri2ro(_globalenv)
baseenv = conversion.converter.ri2ro(rinterface.baseenv)
emptyenv = conversion.converter.ri2ro(rinterface.emptyenv)

