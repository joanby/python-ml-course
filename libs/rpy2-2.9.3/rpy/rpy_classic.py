"""
Implementation of RPy 1.x (for compatibility)
"""

import rpy2.rinterface as ri
import array

#
RPY_VERSION = '1.x'

# --- options in 'rpy_options.py'

rpy_options = {
    'RHOME':None,       # R Installation Directory
    'RVERSION':None,    # R Version *string*
    'RVER':None,        # R Version *number*
    'RUSER':None,       # R User's Home Directory
    'USE_NUMERIC':None, # Is Numeric module available
    'VERBOSE':False,    # Should status messages be displated.
    'SETUP_READ_CONSOLE':  True,  # False for no standard console read config
    'SETUP_WRITE_CONSOLE': True,  # False for no standard console write config
    'SETUP_SHOWFILES':     True   # False for no standard console file viewerd config
    }

# --- more options

VERBOSE = True
RVERSION = None
RHOME = None

TOP_CONVERSION    = 4
PROC_CONVERSION   = 4
CLASS_CONVERSION  = 3
BASIC_CONVERSION  = 2
VECTOR_CONVERSION = 1
NO_CONVERSION     = 0
NO_DEFAULT        = -1

# from RPy.h
TOP_MODE = 4


# --- init R

ri.initr()

class RPyException(Exception):
    """ Generic exeception for RPy """
    pass
class RPyTypeConversionException(RPyException):
    pass
class RPyRException(RuntimeError):
    """ Runtime error while running R code. """
    pass

# for backwards compatibility
RException = RPyException

# I/O setters
# FIXME: sort out the details of what RPy is doing with that
# and what is the amount that is user-defined (and/or used).
set_rpy_output = None
set_rpy_input = None
get_rpy_output = None
get_rpy_input = None


# --- "CONVERSION" system


# same default as in "rpymodule.c"
default_mode = -1

# Wrap a function in safe modes to avoid infinite recursion when
# called from within the conversion system
def with_mode(i, fun):
    def f(*args, **kwds):
        try:
            e = get_default_mode()
            set_default_mode(i)
            return fun(*args, **kwds)
        finally:
            set_default_mode(e)
    return f

# Manage the global mode
def set_default_mode(mode):
    if not isinstance(mode, int):
        raise ValueError("mode should be an int.")
    if (mode < -1) or (mode > TOP_MODE):
        raise ValueError("wrong mode.")
    global default_mode
    default_mode = mode

def get_default_mode():
    global default_mode
    return default_mode



# note: inherits from dict: not considering pre-2.2 versions of Python
class Dict_With_Mode(dict):
    def __setitem__(self, key, value):
        v = with_mode(BASIC_CONVERSION, value)
        if type(key) not in [str, tuple]:
            k = with_mode(BASIC_CONVERSION, key)
        
        super(Dict_With_Mode, self).__setitem__(k, v)

proc_table = Dict_With_Mode({})
class_table = Dict_With_Mode({})


def seq2vec(seq):
    types = [bool, int, float, str]
    has_type = [False, False, False, False]
    for tp_i, tp in enumerate(types):
        for elt in seq:
            if isinstance(elt, tp):
                has_type[tp_i] = True
    r_type = None
    if has_type[3]:
        r_type = ri.STRSXP
    elif has_type[2]:
        r_type = ri.REALSXP
    elif has_type[1]:
        r_type = ri.INTSXP
    elif has_type[0]:
        r_type = ri.LGLSXP
    if r_type is not None:
        vec = ri.SexpVector(seq, r_type)
    return vec
            
def py2rpy(obj):
    if isinstance(obj, int):
        robj = ri.SexpVector([obj, ], ri.INTSXP)
        return robj 
    if isinstance(obj, float):
        robj = ri.SexpVector([obj, ], ri.REALSXP)
        return robj 
    if isinstance(obj, str):
        robj = ri.SexpVector([obj, ], ri.STRSXP)
        return robj 
    if isinstance(obj, complex):
        robj = ri.SexpVector([obj, ], ri.CPLSXP)
        return robj 
    if isinstance(obj, list) or isinstance(obj, tuple):
        robj = seq2vec(obj)
        return robj
    raise ValueError("Don't know what to do with 'obj'.")

def rpy2py_basic(obj):    
    if hasattr(obj, '__len__'):
        if obj.typeof in [ri.INTSXP, ri.REALSXP, ri.CPLXSXP,
                          ri.LGLSXP,ri.STRSXP]:
            res = [x for x in obj]
        elif obj.typeof in [ri.VECSXP]:
            try:
                # if the returned objects is a list with names, return a dict
                obj_names = obj.do_slot("names")
                # caution: throw an exception if duplicated names
                if (len(set(obj_names)) != len(obj_names)):
                    raise ValueError("Duplicated names in the R named list.")
                res = dict([(obj_names[i], rpy2py(x)) for i,x in enumerate(obj)])
            except LookupError:
                res = [rpy2py(x) for x in obj]
        elif obj.typeof == [ri.LANGSXP]:
            res = Robj(obj)
        else:
            raise ValueError("Invalid type for 'obj'.")
    else:
        res = Robj(obj)
    return res
    #raise ValueError("Invalid type for 'obj'.")

def rpy2py(obj, mode=None):
    """ Transform RPy objects into pure python objects. """
    if mode is None:
        mode = default_mode
    if mode == NO_CONVERSION:
        res = Robj(obj)
        return res
    if mode == BASIC_CONVERSION:
        res = rpy2py_basic(obj)
        return res
    raise ValueError("Invalid default mode.")

class Robj(object):
    """ Class to model any R object. 
    As in the 'classic' RPy (that is versions 1.x),
    Whether an object is callable or a vector, or else, is
    resolved at runtime in R and it only exposed as an "R something"
    to Python.
    """

    __local_mode = NO_DEFAULT

    def __init__(self, sexp):

        if not isinstance(sexp, ri.Sexp):
            raise ValueError('"sexp" must inherit from rinterface.Sexp (not %s)' %str(type(sexp)))
        self.__sexp = sexp

    def __call__(self, *args, **kwargs):
        args_r = []
        for a in args:
            if isinstance(a, ri.Sexp):
                a = a
            elif isinstance(a, Robj):
                a = a.get_sexp()
            else:
                a = py2rpy(a)
            args_r.append(a)
        kwargs_r = {}
        for a_n in kwargs:
            a = kwargs[a_n]
            if isinstance(a, ri.Sexp):
                a = a
            elif isinstance(a, Robj):
                a = a.get_sexp()
            else:                
                a = py2rpy(a)
            kwargs_r[a_n] = a

        res = self.__sexp(*args_r, **kwargs_r)
        res = rpy2py(res)
        return res

    def __getitem__(self, item):
        if not isinstance(item, Robj):
            item = py2rpy(item)
        res = r["["](self.__sexp, item)
        mode = self.__local_mode
        if mode == BASIC_CONVERSION:
            res = rpy2py(res)
        return res

    ##FIXME: not part of RPy-1.x.
    def get_sexp(self):
        return self.__sexp

    sexp = property(fget = get_sexp)
    
    #def __repr__(self):
    #    res = rpy2py(self)
    #    return res

    def as_py(self, mode = None):
        if mode is None:
            mode = default_mode
        res = rpy2py(self.__sexp, mode = mode)
        return res

    def __local_mode(self, mode = default_mode):
        self.__local_mode = mode


class R(object):
    def __init__(self):
        self.get = ri.globalenv.get
        self.TRUE = ri.TRUE
        self.FALSE = ri.FALSE
        
        
    def __getattr__(self, name):
        if name.startswith('__') and name.endswith('__'):
            return super(R, self).__getattr__(name)
        if len(name) > 1 and name[-1] == '_' and name[-2] != '_':
            name = name[:-1]
        name = name.replace('__', '<-')
        name = name.replace('_', '.')
        res = self.__getitem__(name)
        return res

    def __getitem__(self, name):
        #FIXME: "get function only" vs "get anything"
        # wantfun = True ?
        res = ri.globalenv.get(name)
        res = rpy2py(res)
        return res

    def __call__(self, s):
        return self.eval(self.parse(text=s))

    def __help__(self, *args, **kwargs):
        helpobj.helpfun(*arg, **kw)
        
    def __repr__(self):
        r_version = ri.baseenv['R.version.string'][0]
        res = 'RPy version %s with %s' %(RPY_VERSION, r_version)
        return res

    def __str__(self):
        return repr(self)

    def __cleanup__(self):
        ri.endEmbeddedR()
        del(self)

r = R()
