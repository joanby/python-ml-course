import rpy2.robjects as ro
import rpy2.robjects.conversion as conversion
import rpy2.rinterface as rinterface
from rpy2.rinterface import (Sexp,
                             SexpVector,
                             ListSexpVector,
                             StrSexpVector, ByteSexpVector,
                             LGLSXP, INTSXP, REALSXP, CPLXSXP,
                             STRSXP, VECSXP, NULL)
import numpy

#from rpy2.robjects.vectors import DataFrame, Vector, ListVector

original_converter = None

# The possible kind codes are listed at
#   http://numpy.scipy.org/array_interface.shtml
_kinds = {
    # "t" -> not really supported by numpy
    "b": rinterface.LGLSXP,
    "i": rinterface.INTSXP,
    # "u" -> special-cased below
    "f": rinterface.REALSXP,
    "c": rinterface.CPLXSXP,
    # "O" -> special-cased below
    "S": rinterface.STRSXP,
    "U": rinterface.STRSXP,
    # "V" -> special-cased below
    #FIXME: datetime64 ?
    #"datetime64":
    }

#FIXME: the following would need further thinking & testing on
#       32bits architectures
_kinds['float64'] = rinterface.REALSXP

_vectortypes = (rinterface.LGLSXP,
                rinterface.INTSXP,
                rinterface.REALSXP,
                rinterface.CPLXSXP,
                rinterface.STRSXP)

converter = conversion.Converter('original numpy conversion')
py2ri = converter.py2ri
py2ro = converter.py2ro
ri2py = converter.ri2py
ri2ro = converter.ri2ro

import sys

if sys.version_info[0] == 3:
    def numpy_O_py2ri(o):
        if all(isinstance(x, str) for x in o):
            res = StrSexpVector(o)
        elif all(isinstance(x, bytes) for x in o):
            res = ByteSexpVector(o)
        else:
            res = conversion.py2ri(list(o))
        return res
else:
    def numpy_O_py2ri(o):
        if all((isinstance(x, str) or isinstance(x, bytes) or isinstance(x, unicode)) for x in o):
            res = StrSexpVector(o)
        else:
            res = conversion.py2ri(list(o))
        return res
    
@py2ri.register(numpy.ndarray)
def numpy2ri(o):
    """ Augmented conversion function, converting numpy arrays into
    rpy2.rinterface-level R structures. """
    if not o.dtype.isnative:
        raise(ValueError("Cannot pass numpy arrays with non-native byte orders at the moment."))

    # Most types map onto R arrays:
    if o.dtype.kind in _kinds:
        # "F" means "use column-major order"
        vec = SexpVector(o.ravel("F"), _kinds[o.dtype.kind])
        dim = SexpVector(o.shape, INTSXP)
        #FIXME: no dimnames ?
        #FIXME: optimize what is below needed/possible ? (other ways to create R arrays ?)
        res = rinterface.baseenv['array'](vec, dim=dim)
    # R does not support unsigned types:
    elif o.dtype.kind == "u":
        raise(ValueError("Cannot convert numpy array of unsigned values -- R does not have unsigned integers."))
    # Array-of-PyObject is treated like a Python list:
    elif o.dtype.kind == "O":
        res = numpy_O_py2ri(o)
    # Record arrays map onto R data frames:
    elif o.dtype.kind == "V":
        if o.dtype.names is None:
            raise(ValueError("Nothing can be done for this numpy array type %s at the moment." % (o.dtype,)))
        df_args = []
        for field_name in o.dtype.names:
            df_args.append((field_name,
                            conversion.py2ri(o[field_name])))
        res = ro.baseenv["data.frame"].rcall(tuple(df_args), ro.globalenv)
    # It should be impossible to get here:
    else:
        raise(ValueError("Unknown numpy array type '%s'." % str(o.dtype)))
    return res

@py2ri.register(numpy.integer)
def npint_py2ri(obj):
    return ro.int2ri(obj)

@py2ri.register(numpy.floating)
def npfloat_py2ri(obj):
    return rinterface.SexpVector([obj, ], rinterface.REALSXP)

@py2ri.register(object)
def nonnumpy2ri(obj):
    # allow array-like objects to also function with this module.
    if not isinstance(obj, numpy.ndarray) and hasattr(obj, '__array__'):
        obj = obj.__array__()
        return ro.default_converter.py2ri(obj)
    elif original_converter is None:
        # This means that the conversion module was not "activated".
        # For now, go with the default_converter.
        # TODO: the conversion system needs an overhaul badly.
        return ro.default_converter.py2ri(obj)
    else:
        # The conversion module was "activated"
        return original_converter.py2ri(obj)

@py2ro.register(numpy.ndarray)
def numpy2ro(obj):
    res = numpy2ri(obj)
    return ro.vectors.rtypeof2rotype[res.typeof](res)

@ri2py.register(ListSexpVector)
def ri2py_list(obj):
    if 'data.frame' in obj.rclass:
        # R "factor" vectors will not convert well by default
        # (will become integers), so we build a temporary list o2
        # with the factors as strings.
        o2 = list()
        # An added complication is that the conversion defined
        # in this module will make __getitem__ at the robjects
        # level return numpy arrays
        for column in rinterface.ListSexpVector(obj):
            if 'factor' in column.rclass:
                levels = tuple(column.do_slot("levels"))
                column = tuple(levels[x-1] for x in column)
            o2.append(column)
        names = obj.do_slot('names')
        if names is NULL:
            res = numpy.rec.fromarrays(o2)
        else:
            res = numpy.rec.fromarrays(o2, names=tuple(names))
    else:
        # not a data.frame, yet is it still possible to convert it
        res = ro.default_converter.ri2py(obj)
    return res

@ri2py.register(Sexp)
def ri2py_sexp(obj):
    if (obj.typeof in _vectortypes) and (obj.typeof != VECSXP):
        res = numpy.asarray(obj)
    else:
        res = ro.default_converter.ri2py(obj)
    return res

def activate():
    global original_converter

    # If module is already activated, there is nothing to do
    if original_converter is not None:
        return

    original_converter = conversion.converter
    new_converter = conversion.Converter('numpy conversion',
                                         template=original_converter)

    for k,v in py2ri.registry.items():
        if k is object:
            continue
        new_converter.py2ri.register(k, v)

    for k,v in ri2ro.registry.items():
        if k is object:
            continue
        new_converter.ri2ro.register(k, v)

    for k,v in py2ro.registry.items():
        if k is object:
            continue
        new_converter.py2ro.register(k, v)

    for k,v in ri2py.registry.items():
        if k is object:
            continue
        new_converter.ri2py.register(k, v)

    conversion.set_conversion(new_converter)


def deactivate():
    global original_converter
    # If module has never been activated or already deactivated,
    # there is nothing to do
    if original_converter is None:
        return

    conversion.set_conversion(original_converter)
    original_converter = None
