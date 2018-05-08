"""This module handles the conversion of data structures
between R objects handled by rpy2 and pandas objects."""

import os
import dateutil.tz
from datetime import datetime
import rpy2.robjects as ro
import rpy2.robjects.conversion as conversion
import rpy2.rinterface as rinterface
from rpy2.rinterface import (SexpVector,
                             StrSexpVector,
                             IntSexpVector,
                             INTSXP)

from pandas.core.frame import DataFrame as PandasDataFrame
from pandas.core.series import Series as PandasSeries
from pandas.core.index import Index as PandasIndex
import pandas
from numpy import recarray
import numpy
import pytz
import tzlocal
import warnings

from collections import OrderedDict
from rpy2.robjects.vectors import (DataFrame,
                                   Vector,
                                   FactorVector,
                                   FloatSexpVector,
                                   ListVector,
                                   StrVector,
                                   IntVector,
                                   POSIXct)
from rpy2.rinterface import (IntSexpVector,
                             ListSexpVector)
original_converter = None 

# pandas is requiring numpy. We add the numpy conversion will be
# activate in the function activate() below
import rpy2.robjects.numpy2ri as numpy2ri

ISOdatetime = rinterface.baseenv['ISOdatetime']
as_vector = rinterface.baseenv['as.vector']

converter = conversion.Converter('original pandas conversion')
py2ri = converter.py2ri
py2ro = converter.py2ro
ri2py = converter.ri2py
ri2ro = converter.ri2ro

# numpy types for Pandas columns that require (even more) special handling
dt_datetime64ns_type = numpy.dtype('datetime64[ns]')
dt_O_type = numpy.dtype('O')

default_timezone = None

@py2ri.register(PandasDataFrame)
def py2ri_pandasdataframe(obj):
    od = OrderedDict()
    for name, values in obj.iteritems():
        try:
            od[name] = conversion.py2ri(values)
        except Exception as e:
            warnings.warn('Error while trying to convert '
                          'the column "%s". Fall back to string conversion. '
                          'The error is: %s' %\
                          (name, str(e)))
            od[name] = StrVector(values)
        
    return DataFrame(od)

@py2ri.register(PandasIndex)
def py2ri_pandasindex(obj):
    if obj.dtype.kind == 'O':
        return StrVector(obj)
    else:
        # pandas2ri should definitely not have to know which paths remain to be
        # converted by numpy2ri
        # Answer: the thing is that pandas2ri builds on the conversion
        # rules defined by numpy2ri - deferring to numpy2ri is allowing
        # us to reuse that code.
        return numpy2ri.numpy2ri(obj)


def py2ri_categoryseries(obj):
    for c in obj.cat.categories:
        if not isinstance(c, str):
            raise ValueError('Converting pandas "Category" series to R factor is only possible when categories are strings.')
    res = IntSexpVector(list(x+1 for x in obj.cat.codes))
    res.do_slot_assign('levels', StrSexpVector(obj.cat.categories))
    if obj.cat.ordered:
        res.rclass = StrSexpVector(('ordered', 'factor'))
    else:
        res.rclass = StrSexpVector(('factor',))
    return res

@py2ri.register(PandasSeries)
def py2ri_pandasseries(obj):
    if numpy.dtype.name == 'O':
        warnings.warn('Element "%s" is of dtype "O" and converted to R vector of strings.' % obj.name)
        res = StrVector(obj)
    elif obj.dtype.name == 'category':
        res = py2ri_categoryseries(obj)
        res = FactorVector(res)
    elif obj.dtype == dt_datetime64ns_type:
        # time series
        d = [IntVector([x.year for x in obj]),
             IntVector([x.month for x in obj]),
             IntVector([x.day for x in obj]),
             IntVector([x.hour for x in obj]),
             IntVector([x.minute for x in obj]),
             IntVector([x.second for x in obj])]
        res = ISOdatetime(*d)
        #FIXME: can the POSIXct be created from the POSIXct constructor ?
        # (is '<M8[ns]' mapping to Python datetime.datetime ?)
        res = POSIXct(res)
    else:
        # converted as a numpy array
        func = numpy2ri.converter.py2ri.registry[numpy.ndarray]
        # current conversion as performed by numpy
        res = func(obj)
        if len(obj.shape) == 1:
            if (obj.dtype != dt_O_type):
                # force into an R vector
                res=as_vector(res)

    # "index" is equivalent to "names" in R
    if obj.ndim == 1:
        res.do_slot_assign('names',
                           StrVector(tuple(str(x) for x in obj.index)))
    else:
        res.do_slot_assign('dimnames',
                           SexpVector(conversion.py2ri(obj.index)))
    return res

@ri2py.register(SexpVector)
def ri2py_vector(obj):
    res = numpy2ri.ri2py(obj)
    return res
    
@ri2py.register(IntSexpVector)
def ri2py_intvector(obj):
    # special case for factors
    if 'factor' in obj.rclass:
        res = pandas.Categorical.from_codes(numpy.asarray(obj) - 1,
                                            categories = obj.do_slot('levels'),
                                            ordered = 'ordered' in obj.rclass)
    else:
        res = numpy2ri.ri2py(obj)
    return res


def get_timezone():
    """ Return the system's timezone settings. """
    if default_timezone:
        timezone = default_timezone
    else:
        timezone = tzlocal.get_localzone()
    return timezone


@ri2py.register(FloatSexpVector)
def ri2py_floatvector(obj):
    # special case for POSIXct date objects
    if 'POSIXct' in obj.rclass:
        tzone_name = obj.do_slot('tzone')[0]
        if tzone_name == '':
            # R is implicitly using the local timezone, while Python time libraries
            # will assume UTC.
            tzone = get_timezone()
        else:
            tzone = pytz.timezone(tzone_name)
        foo = (tzone.localize(datetime.fromtimestamp(x)) for x in obj)
        res = pandas.to_datetime(tuple(foo))
    else:
        res = numpy2ri.ri2py(obj)
    return res

@ri2py.register(ListSexpVector)
def ri2py_listvector(obj):        
    if 'data.frame' in obj.rclass:
        res = ri2py(DataFrame(obj))
    else:
        res = numpy2ri.ri2py(obj)
    return res

@ri2py.register(DataFrame)
def ri2py_dataframe(obj):
    items = tuple((k, ri2py(v)) for k, v in obj.items())
    res = PandasDataFrame.from_items(items)
    return res

def activate():
    global original_converter
    # If module is already activated, there is nothing to do
    if original_converter is not None: 
        return

    original_converter = conversion.Converter('snapshot before pandas conversion',
                                              template=conversion.converter)
    numpy2ri.activate()
    new_converter = conversion.Converter('snapshot before pandas conversion',
                                         template=conversion.converter)
    numpy2ri.deactivate()

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

