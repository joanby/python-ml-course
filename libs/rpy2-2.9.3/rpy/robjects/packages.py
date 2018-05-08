import os
import warnings
from types import ModuleType
from collections import defaultdict
from warnings import warn
import rpy2.rinterface as rinterface
import rpy2.robjects.lib
from . import conversion
from rpy2.robjects.functions import (SignatureTranslatedFunction, 
                                     docstring_property, 
                                     DocumentedSTFunction)
from rpy2.robjects.constants import NULL
from rpy2.robjects import Environment
from rpy2.robjects.packages_utils import (_libpaths, 
                                          get_packagepath, 
                                          _packages,
                                          default_symbol_r2python,
                                          default_symbol_check_after,
                                          _map_symbols,
                                          _fix_map_symbols)
import rpy2.robjects.help as rhelp

_require = rinterface.baseenv['require']
_library = rinterface.baseenv['library']
_as_env = rinterface.baseenv['as.environment']
_package_has_namespace = rinterface.baseenv['packageHasNamespace']
_system_file = rinterface.baseenv['system.file']
_get_namespace = rinterface.baseenv['getNamespace']
_get_namespace_version = rinterface.baseenv['getNamespaceVersion']
_get_namespace_exports = rinterface.baseenv['getNamespaceExports']
_loaded_namespaces = rinterface.baseenv['loadedNamespaces']
_globalenv = rinterface.globalenv
_new_env = rinterface.baseenv["new.env"]

StrSexpVector = rinterface.StrSexpVector
# Fetching symbols in the namespace "utils" assumes that "utils" is loaded
# (currently the case by default in R).
_data = rinterface.baseenv['::'](StrSexpVector(('utils', )),
                                 StrSexpVector(('data', )))

_reval = rinterface.baseenv['eval']
_options = rinterface.baseenv['options']


def no_warnings(func):
    """ Decorator to run R functions without warning. """
    def run_withoutwarnings(*args, **kwargs):
        warn_i = _options().do_slot('names').index('warn')
        oldwarn = _options()[warn_i][0]
        _options(warn = -1)
        try:
            res = func(*args, **kwargs)
        except Exception as e:
            # restore the old warn setting before propagating
            # the exception up
            _options(warn = oldwarn)
            raise e
        _options(warn = oldwarn)
        return res
    return run_withoutwarnings

@no_warnings
def _eval_quiet(expr):
    return _reval(expr)

# FIXME: should this be part of the API for rinterface ?
#        (may be it is already the case and there is code
#        duplicaton ?)
def reval(string, envir = _globalenv):
    """ Evaluate a string as R code
    :param string: R code
    :type string: a :class:`str`
    :param envir: an environment in which the environment should take place (default: R's global environment)
    """
    p = rinterface.parse(string)
    res = _reval(p, envir = envir)
    return res

def quiet_require(name, lib_loc = None):
    """ Load an R package /quietly/ (suppressing messages to the console). """
    if lib_loc == None:
        lib_loc = "NULL"
    else:
        lib_loc = "\"%s\"" % (lib_loc.replace('"', '\\"'))
    expr_txt = "suppressPackageStartupMessages(base::require(%s, lib.loc=%s))" \
        %(name, lib_loc)
    expr = rinterface.parse(expr_txt)
    ok = _eval_quiet(expr)
    return ok


class PackageData(object):
    """ Datasets in an R package.
    In R datasets can be distributed with a package.

    Datasets can be:

    - serialized R objects

    - R code (that produces the dataset)

    For a given R packages, datasets are stored separately from the rest
    of the code and are evaluated/loaded lazily.

    The lazy aspect has been conserved and the dataset are only loaded
    or generated when called through the method 'fetch()'.
    """
    _packagename = None
    _lib_loc = None
    _datasets = None
    def __init__(self, packagename, lib_loc = rinterface.NULL):
        self._packagename = packagename
        self._lib_loc

    def _init_setlist(self):
        _datasets = dict()
        # 2D array of information about datatsets
        tmp_m = _data(**{'package':StrSexpVector((self._packagename, )),
                         'lib.loc': self._lib_loc})[2]
        nrows, ncols = tmp_m.do_slot('dim')
        c_i = 2
        for r_i in range(nrows):
            _datasets[tmp_m[r_i + c_i * nrows]] = None
            # FIXME: check if instance methods are overriden
        self._datasets = _datasets

    def names(self):
        """ Names of the datasets"""
        if self._datasets is None:
            self._init_setlist()
        return self._datasets.keys()
    
    def fetch(self, name):
        """ Fetch the dataset (loads it or evaluates the R associated
        with it.

        In R, datasets are loaded into the global environment by default
        but this function returns an environment that contains the dataset(s).
        """
        #tmp_env = rinterface.SexpEnvironment()
        if self._datasets is None:
            self._init_setlist()

        if name not in self._datasets:
            raise ValueError('Data set "%s" cannot be found' % name)
        env = _new_env()
        _data(StrSexpVector((name, )),
              **{'package': StrSexpVector((self._packagename, )),
                 'lib.loc': self._lib_loc,
                 'envir': env})
        return Environment(env)

class Package(ModuleType):
    """ Models an R package
    (and can do so from an arbitrary environment - with the caution
    that locked environments should mostly be considered).
     """
    
    _env = None
    __rname__ = None
    _translation = None
    _rpy2r = None
    __fill_rpy2r__ = None
    __update_dict__ = None
    _exported_names = None
    _symbol_r2python = None
    __version__ = None
    __rdata__ = None

    def __init__(self, env, name, translation = {}, 
                 exported_names = None, on_conflict = 'fail',
                 version = None,
                 symbol_r2python = default_symbol_r2python,
                 symbol_check_after = default_symbol_check_after):
        """ Create a Python module-like object from an R environment,
        using the specified translation if defined. 

        - env: R environment
        - name: package name
        - translation: `dict` with R names as keys and corresponding Python
                       names as values
        - exported_names: `set` of names/symbols to expose to instance users
        - on_conflict: 'fail' or 'warn' (default: 'fail')
        - version: version string for the package
        - symbol_r2python: function to convert R symbols into Python symbols.
                           The default translate `.` into `_`.
        - symbol_check_after: function to check the Python symbols obtained
                              from `symbol_r2python`.
        """

        super(Package, self).__init__(name)
        self._env = env
        self.__rname__ = name
        self._translation = translation
        mynames = tuple(self.__dict__)
        self._rpy2r = {}
        if exported_names is None:
            exported_names = set(self._env.keys())
        self._exported_names = exported_names
        self._symbol_r2python = symbol_r2python
        self._symbol_check_after = symbol_check_after
        self.__fill_rpy2r__(on_conflict = on_conflict)
        self._exported_names = self._exported_names.difference(mynames)
        self.__version__ = version
                
    def __update_dict__(self, on_conflict = 'fail'):
        """ Update the __dict__ according to what is in the R environment """
        for elt in self._rpy2r:
            del(self.__dict__[elt])
        self._rpy2r.clear()
        self.__fill_rpy2r__(on_conflict = on_conflict)

    def __fill_rpy2r__(self, on_conflict = 'fail'):
        """ Fill the attribute _rpy2r.

        - on_conflict: 'fail' or 'warn' (default: 'fail')
        """

        assert(on_conflict in ('fail', 'warn'))

        name = self.__rname__

        (symbol_mapping, 
         conflicts, 
         resolutions) = _map_symbols(self._env,
                                     translation = self._translation,
                                     symbol_r2python = self._symbol_r2python,
                                     symbol_check_after = self._symbol_check_after)
        msg_prefix = 'Conflict when converting R symbols'+\
                     ' in the package "%s"' % self.__rname__ +\
                     ' to Python symbols: \n-'
        exception = LibraryError
        _fix_map_symbols(symbol_mapping,
                         conflicts,
                         on_conflict,
                         msg_prefix,
                         exception)
        symbol_mapping.update(resolutions)
        reserved_pynames = set(dir(self))
        for rpyname, rnames in symbol_mapping.items():
            # last paranoid check
            if len(rnames) > 1:
                raise ValueError('Only one R name should be associated with %s (and we have %s)' % (rpyname, str(rnames)))
            rname = rnames[0]
            if rpyname in reserved_pynames:
                raise LibraryError('The symbol ' + rname +\
                                   ' in the package "' + name + '"' +\
                                   ' is conflicting with' +\
                                   ' a Python object attribute')
            self._rpy2r[rpyname] = rname
            if (rpyname != rname) and (rname in self._exported_names):
                self._exported_names.remove(rname)
                self._exported_names.add(rpyname)
            try:
                riobj = self._env[rname]
            except rinterface.RRuntimeError as rre:
                warn(str(rre))
            rpyobj = conversion.ri2ro(riobj)
            if hasattr(rpyobj, '__rname__'):
                rpyobj.__rname__ = rname
            #FIXME: shouldn't the original R name be also in the __dict__ ?
            self.__dict__[rpyname] = rpyobj

    def __repr__(self):
        s = super(Package, self).__repr__()
        return 'rpy2.robjects.packages.Package as a ' + s

# alias
STF = SignatureTranslatedFunction

class SignatureTranslatedPackage(Package):
    """ R package in which the R functions had their signatures 
    'translated' (that this the named parameters were made to 
    to conform Python's rules for vaiable names)."""
    def __fill_rpy2r__(self, on_conflict = 'fail'):
        super(SignatureTranslatedPackage, self).__fill_rpy2r__(on_conflict = on_conflict)
        for name, robj in self.__dict__.items():
            if isinstance(robj, rinterface.Sexp) and robj.typeof == rinterface.CLOSXP:
                self.__dict__[name] = STF(self.__dict__[name],
                                          on_conflict = on_conflict,
                                          symbol_r2python = self._symbol_r2python,
                                          symbol_check_after = self._symbol_check_after)

# alias
STP = SignatureTranslatedPackage

class SignatureTranslatedAnonymousPackage(SignatureTranslatedPackage):
    def __init__(self, string, name):
        env = Environment()
        reval(string, env)
        super(SignatureTranslatedAnonymousPackage, self).__init__(env,
                                                                  name)

# alias
STAP = SignatureTranslatedAnonymousPackage

class InstalledSTPackage(SignatureTranslatedPackage):
    @docstring_property(__doc__)
    def __doc__(self):
        doc = list(['Python representation of an R package.'])
        if not self.__rname__:
            doc.append('<No information available>')
        else:
            try:
                doc.append(rhelp.docstring(self.__rname__,
                                           self.__rname__ + '-package',
                                           sections=['description']))
            except rhelp.HelpNotFoundError as hnf:
                doc.append('[R help was not found]')
        return os.linesep.join(doc)

    def __fill_rpy2r__(self, on_conflict = 'fail'):
        super(SignatureTranslatedPackage, self).__fill_rpy2r__(on_conflict = on_conflict)
        for name, robj in self.__dict__.items():
            if isinstance(robj, rinterface.Sexp) and robj.typeof == rinterface.CLOSXP:
                self.__dict__[name] = DocumentedSTFunction(self.__dict__[name],
                                                           packagename = self.__rname__)


class InstalledPackage(Package):
    @docstring_property(__doc__)
    def __doc__(self):
        doc = list(['Python representation of an R package.',
                    'R arguments:', ''])
        if not self.__rname__:
            doc.append('<No information available>')
        else:
            try:
                doc.append(rhelp.docstring(self.__rname__,
                                           self.__rname__ + '-package',
                                           sections=['description']))
            except rhelp.HelpNotFoundError as hnf:
                doc.append('[R help was not found]')
        return os.linesep.join(doc)


class WeakPackage(Package):
    """
    'Weak' R package, with which looking for symbols results in
    a warning (and a None returned) whenever the desired symbol is
    not found (rather than a traditional `AttributeError`).
    """
    
    def __getattr__(self, name):
        res =self.__dict__.get(name)
        if res is None:
            warnings.warn("The symbol '%s' is not in this R namespace/package." % name)
        return res
    
class LibraryError(ImportError):
    """ Error occuring when importing an R library """
    pass


class InstalledPackages(object):
    """ R packages installed. """
    def __init__(self, lib_loc=None):
        libraryiqr =  _library(**{'lib.loc': lib_loc})
        lib_results_i = libraryiqr.do_slot('names').index('results')
        self.lib_results = libraryiqr[lib_results_i]
        self.nrows, self.ncols = self.lib_results.do_slot('dim')
        self.colnames = self.lib_results.do_slot('dimnames')[1] # column names
        self.lib_packname_i = self.colnames.index('Package')

    def isinstalled(self, packagename):
        if not isinstance(packagename, rinterface.StrSexpVector):
            rname = rinterface.StrSexpVector((packagename, ))
        else:
            if len(packagename) > 1:
                raise ValueError("Only specify one package name at a time.")
            rname = packagename
        nrows, ncols = self.nrows, self.ncols
        lib_results, lib_packname_i = self.lib_results, self.lib_packname_i
        for i in range(0+lib_packname_i*nrows, 
                       nrows*(lib_packname_i+1), 
                       1):
            if lib_results[i] == packagename:
                return True
        return False

    def __iter__(self):
        """ Iterate through rows, yield tuples at each iteration """
        lib_results = self.lib_results
        nrows, ncols = self.nrows, self.ncols
        colrg = range(0, ncols)
        for row_i in range(nrows):
            yield tuple(lib_results[x*nrows+row_i] for x in colrg)

def isinstalled(name,
                lib_loc = None):
    """
    Find whether an R package is installed 
    :param name: name of an R package
    :param lib_loc: specific location for the R library (default: None)

    :rtype: a :class:`bool`
    """
    
    instapack = InstalledPackages(lib_loc)
    return instapack.isinstalled(name)

def importr(name, 
            lib_loc = None,
            robject_translations = {}, 
            signature_translation = True,
            suppress_messages = True,
            on_conflict = 'fail',
            symbol_r2python = default_symbol_r2python,
            symbol_check_after = default_symbol_check_after,
            data = True):
    """ Import an R package.

    Arguments:

    - name: name of the R package

    - lib_loc: specific location for the R library (default: None)

    - robject_translations: dict (default: {})

    - signature_translation: (True or False)

    - suppress_message: Suppress messages R usually writes on the console
      (defaut: True)

    - on_conflict: 'fail' or 'warn' (default: 'fail')

    - symbol_r2python: function to translate R symbols into Python symbols

    - symbol_check_after: function to check the Python symbol obtained
                          from `symbol_r2python`.

    - data: embed a PackageData objects under the attribute 
      name __rdata__ (default: True)

    Return:

    - an instance of class SignatureTranslatedPackage, or of class Package 

    """

    rname = rinterface.StrSexpVector((name, ))

    if suppress_messages:
        ok = quiet_require(name, lib_loc = lib_loc)
    else:
        ok = _require(rinterface.StrSexpVector(rname), 
                      **{'lib.loc': rinterface.StrSexpVector((lib_loc, ))})[0]
    if not ok:
        raise LibraryError("The R package %s could not be imported" %name)
    if _package_has_namespace(rname, 
                              _system_file(package = rname)):
        env = _get_namespace(rname)
        version = _get_namespace_version(rname)[0]
        exported_names = set(_get_namespace_exports(rname))
    else:
        env = _as_env(rinterface.StrSexpVector(['package:'+name, ]))
        exported_names = None
        version = None

    if signature_translation:
        pack = InstalledSTPackage(env, name, 
                                  translation = robject_translations,
                                  exported_names = exported_names,
                                  on_conflict = on_conflict,
                                  version = version,
                                  symbol_r2python = symbol_r2python,
                                  symbol_check_after = symbol_check_after)
    else:
        pack = InstalledPackage(env, name, translation = robject_translations,
                                exported_names = exported_names,
                                on_conflict = on_conflict,
                                version = version,
                                symbol_r2python = symbol_r2python,
                                symbol_check_after = symbol_check_after)
    if data:
        if pack.__rdata__ is not None:
            warn('While importing the R package "%s", the rpy2 Package object is masking a translated R symbol "__rdata__" already present' % name)
        pack.__rdata__ = PackageData(name, lib_loc = lib_loc)

    return pack

def data(package):
    """ Return the PackageData for the given package."""
    return package.__rdata__

def wherefrom(symbol, startenv = rinterface.globalenv):
    """ For a given symbol, return the environment
    this symbol is first found in, starting from 'startenv'.
    """
    env = startenv
    obj = None
    tryagain = True
    while tryagain:
        try:
            obj = env[symbol]
            tryagain = False
        except LookupError as knf:
            env = env.enclos()
            if env.rsame(rinterface.emptyenv):
                tryagain = False
            else:
                tryagain = True
    return conversion.ri2ro(env)

