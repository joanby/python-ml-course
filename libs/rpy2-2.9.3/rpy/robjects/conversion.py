"""
The module contains the conversion functions to be
used by the rpy2.robjects functions and methods.

These functions are initially empty place-holders,
raising a NotImplementedError exception.
"""

import sys
from collections import namedtuple

if sys.version_info[0] < 3 or (sys.version_info[0] == 3 and sys.version_info[1] < 4):
    from singledispatch import singledispatch
else:
    from functools import singledispatch

def overlay_converter(src, target):
    """
    :param src: source of additional conversion rules
    :type src: :class:`Converter`
    :param target: target. The conversion rules in the src will
                   be added to this object.
    :type target: :class:`Converter`
    """
    for k,v in src.ri2ro.registry.items():
        # skip the root dispatch
        if k is object and v is _ri2ro:
            continue
        target._ri2ro.register(k, v)
    for k,v in src.py2ri.registry.items():
        # skip the root dispatch
        if k is object and v is _py2ri:
            continue
        target._py2ri.register(k, v)
    for k,v in src.py2ro.registry.items():
        # skip the root dispatch
        if k is object and v is _py2ro:
            continue
        target._py2ro.register(k, v)
    for k,v in src.ri2py.registry.items():
        # skip the root dispatch
        if k is object and v is _ri2py:
            continue
        target._ri2py.register(k, v)

def _ri2ro(obj):
    """ Dummy function for ri2ro.

    This function will convert rpy2.rinterface (ri) low-level objects
    into rpy2.robjects (ro) higher-level objects.
    """
    raise NotImplementedError("Conversion 'ri2ro' not defined for objects of type '%s'" % str(type(obj)))

def _py2ri(obj):
    """ Dummy function for py2ri.
    
    This function will convert Python objects into rpy2.rinterface
    (ri) objects.
    """
    raise NotImplementedError("Conversion 'py2ri' not defined for objects of type '%s'" % str(type(obj)))

def _py2ro(obj):
    """ Dummy function for py2ro.

    This function will convert Python objects into rpy2.robjects
    (ro) objects.
    """
    raise NotImplementedError("Conversion 'py2ro' not defined for objects of type '%s'" % str(type(obj)))

def _ri2py(obj):
    """ Dummy function for ri2py.

    This function will convert Python objects into Python (presumably non-rpy2) objects.
    """
    raise NotImplementedError("Conversion 'ri2py' not defined for objects of type '%s'" % str(type(obj)))


class Converter(object):
    """
    Conversion between rpy2's low-level and high-level proxy objects
    for R objects, and Python (no R) objects.

    Converter objects can be added, the result being
    a Converter objects combining the translation rules from the
    different converters.
    """
    name = property(lambda self: self._name)
    ri2ro = property(lambda self: self._ri2ro)
    py2ri = property(lambda self: self._py2ri)
    py2ro = property(lambda self: self._py2ro)
    ri2py = property(lambda self: self._ri2py)
    lineage = property(lambda self: self._lineage)
    
    def __init__(self, name,
                 template=None):
        (ri2ro, py2ri, py2ro, ri2py) = Converter.make_dispatch_functions()
        self._name = name
        self._ri2ro = ri2ro
        self._py2ri = py2ri
        self._py2ro = py2ro
        self._ri2py = ri2py

        if template is None:
            lineage = tuple()
        else:
            lineage = list(template.lineage)
            lineage.append(name)
            lineage = tuple(lineage)
            overlay_converter(template, self)
        self._lineage = lineage

    def __add__(self, converter):
        assert isinstance(converter, Converter)
        new_name = '%s + %s' % (self.name, converter.name)
        # create a copy of `self` as the result converter
        result_converter = Converter(new_name, template=self)
        overlay_converter(converter, result_converter)
        return result_converter
    
    @staticmethod
    def make_dispatch_functions():
        ri2ro = singledispatch(_ri2ro)
        py2ri = singledispatch(_py2ri)
        py2ro = singledispatch(_py2ro)
        ri2py = singledispatch(_ri2py)

        return (ri2ro, py2ri, py2ro, ri2py)


class ConversionContext(object):
    """
    Context manager for instances of class Converter.
    """
    def __init__(self, ctx_converter):
        assert isinstance(ctx_converter, Converter)
        self._original_converter = converter
        self.ctx_converter = Converter('Converter-%i-in-context' % id(self),
                                       template=ctx_converter)

    def __enter__(self):
        set_conversion(self.ctx_converter)
        return self.ctx_converter

    def __exit__(self, exc_type, exc_val, exc_tb):
        set_conversion(self._original_converter)
        return False

localconverter = ConversionContext
    
converter = None
py2ri = None
py2ro = None
ri2ro = None
ri2py = None

def set_conversion(this_converter):
    """
    Set conversion rules in the conversion module.
    :param this_converter: The conversion rules
    :type this_converter: :class:`Converter`
    """
    global converter, py2ri, py2ro, ri2ro, ri2py
    converter = this_converter
    py2ri = converter.py2ri
    py2ro = converter.py2ro
    ri2ro = converter.ri2ro
    ri2py = converter.ri2py

set_conversion(Converter('base converter'))
