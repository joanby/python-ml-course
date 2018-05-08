"""
R objects with fixed values.
"""

import rpy2.rinterface as rinterface

_reval = rinterface.baseenv['eval']

# NULL
NULL = _reval(rinterface.parse("NULL"))

# TRUE/FALSE
TRUE = _reval(rinterface.parse("TRUE"))
FALSE = _reval(rinterface.parse("FALSE"))
