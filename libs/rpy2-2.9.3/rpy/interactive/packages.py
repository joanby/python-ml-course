from rpy2.robjects.packages import importr as _importr
from rpy2.robjects.packages import data
import rpy2.robjects.help as rhelp
from rpy2.rinterface import baseenv
from os import linesep
from collections import OrderedDict
import re

class Packages(object):
    __instance = None
    def __new__(cls):
        if cls.__instance is None:
            cls.__instance = object.__new__(cls)
        return cls.__instance

    def __setattr__(self, name, value):
        raise AttributeError("Attributes cannot be set. Use 'importr'")

packages = Packages()
_loaded_namespaces = baseenv['loadedNamespaces']

def importr(packname, newname = None, verbose = False):
    """ Wrapper around rpy2.robjects.packages.importr, 
    adding the following feature(s):
    
    - package instance added to the pseudo-module 'packages'

    """

    assert isinstance(packname, str)
    packinstance = _importr(packname, on_conflict = 'warn')

    # fix the package name (dots possible in R package names)
    if newname is None:
        newname = packname.replace('.', '_')

    Packages().__dict__[newname] = packinstance

    ## Currently too slow for a serious usage: R's introspection 
    ## of S4 classes is not fast enough
    # d = {}
    # for cn in methods.get_classnames(packname):
    #     class AutoS4(RS4):
    #         __metaclass__ = methods.RS4Auto_Type
    #         __rpackagename__ = packname
    #         __rname__ = cn
    #     newcn = cn.replace('.', '_')
    #     d[newcn] = AutoS4
    # S4Classes().__dict__[newname] = d 
    
    return packinstance

for packname in _loaded_namespaces():
    importr(packname)
