"""
Utilities for manipulating or evaluating the R language.
"""

import rpy2.robjects.conversion as conversion
import rpy2.rinterface as ri
_reval = ri.baseenv['eval']
_parse = ri.parse

def eval(x, envir = ri.globalenv):
    """ Evaluate R code. If the input object is an R expression it
    evaluates it directly, if it is a string it parses it before 
    evaluating it.

    By default the evaluation is performed in R's global environment
    but a specific environment can be specified."""
    if isinstance(x, str) or isinstance(x, unicode):
        p = _parse(x)
    else:
        p = x
    res = _reval(p, envir = envir)
    res = conversion.ri2ro(res)
    return res

del(ri)
