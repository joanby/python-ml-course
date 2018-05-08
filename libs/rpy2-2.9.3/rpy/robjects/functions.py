import os, re
from collections import OrderedDict
from rpy2.robjects.robject import RObjectMixin, RObject
import rpy2.rinterface as rinterface
from rpy2.robjects import help
#import rpy2.robjects.conversion conversion
from . import conversion
# XXX: I need to import default_ri2ro

from rpy2.robjects.packages_utils import (default_symbol_r2python,
                                          default_symbol_check_after,
                                          _map_symbols,
                                          _fix_map_symbols)


baseenv_ri = rinterface.baseenv

#needed to avoid circular imports
_reval = rinterface.baseenv['eval']

__formals = baseenv_ri.get('formals')
__args = baseenv_ri.get('args')
#_genericsargsenv = baseenv_ri['.GenericArgsEnv']
#_argsenv = baseenv_ri['.ArgsEnv']
def _formals_fixed(func):
    tmp = __args(func)
    return __formals(tmp)

## docstring_property and DocstringProperty
## from Bradley Froehle
## https://gist.github.com/bfroehle/4041015
def docstring_property(class_doc):
    def wrapper(fget):
        return DocstringProperty(class_doc, fget)
    return wrapper
 
class DocstringProperty(object):
    def __init__(self, class_doc, fget):
        self.fget = fget
        self.class_doc = class_doc
    def __get__(self, obj, objtype=None):
        if obj is None:
            return self.class_doc
        else:
            return self.fget(obj)
    def __set__(self, obj, value):
        raise AttributeError("Cannot set the attribute")
    def __delete__(self, obj):
        raise AttributeError("Cannot delete the attribute")

def _repr_argval(obj):
    """ Helper functions to display an R object in the docstring.
    This a hack and will be hopefully replaced the extraction of
    information from the R help system."""
    try:
        l = len(obj)
        if l == 1:
            if obj[0].rid == rinterface.MissingArg.rid:
                # no default value
                s = None
            elif obj[0].rid == rinterface.NULL.rid:
                s = 'rinterface.NULL'
            else:
                s = str(obj[0][0])
        elif l > 1:
            s = '(%s, ...)' % str(obj[0][0])
        else:
            s = str(obj)
    except:
        s = str(obj)
    return s

class Function(RObjectMixin, rinterface.SexpClosure):
    """ Python representation of an R function.
    """
    __local = baseenv_ri.get('local')
    __call = baseenv_ri.get('call')
    __assymbol = baseenv_ri.get('as.symbol')
    __newenv = baseenv_ri.get('new.env')

    _local_env = None
        
    def __init__(self, *args, **kwargs):
        super(Function, self).__init__(*args, **kwargs)
        self._local_env = self.__newenv(hash=rinterface.BoolSexpVector((True, )))

    @docstring_property(__doc__)
    def __doc__(self):
        fm = _formals_fixed(self)
        doc = list(['Python representation of an R function.',
                    'R arguments:', ''])
        if fm is rinterface.NULL:
            doc.append('<No information available>')
        for key, val in zip(fm.do_slot('names'), fm):
            if key == '...':
                val = 'R ellipsis (any number of parameters)'
            doc.append('%s: %s' % (key, _repr_argval(val)))
        return os.linesep.join(doc)


    def __call__(self, *args, **kwargs):
        new_args = [conversion.py2ri(a) for a in args]
        new_kwargs = {}
        for k, v in kwargs.items():
            new_kwargs[k] = conversion.py2ri(v)
        res = super(Function, self).__call__(*new_args, **new_kwargs)
        res = conversion.ri2ro(res)
        return res

    def formals(self):
        """ Return the signature of the underlying R function 
        (as the R function 'formals()' would).
        """
        res = _formals_fixed(self)
        res = conversion.ri2ro(res)
        return res

    def rcall(self, *args):
        """ Wrapper around the parent method rpy2.rinterface.SexpClosure.rcall(). """
        res = super(Function, self).rcall(*args)
        # XXX: Now that ri2ro converts to python objects, we restrict to the
        # default now for a raw R call. I'm not sure there should be *any*
        # conversion. However, we need to get this out of __init__.py first!
        # res = ro.default_ri2ro(res)
        return res

class SignatureTranslatedFunction(Function):
    """ Python representation of an R function, where
    the names in named argument are translated to valid
    argument names in Python. """
    _prm_translate = None


    def __init__(self, sexp,
                 init_prm_translate = None,
                 on_conflict = 'warn',
                 symbol_r2python = default_symbol_r2python,
                 symbol_check_after = default_symbol_check_after):
        super(SignatureTranslatedFunction, self).__init__(sexp)
        if init_prm_translate is None:
            prm_translate = OrderedDict()
        else:
            assert isinstance(init_prm_translate, dict)
            prm_translate = init_prm_translate

        formals = self.formals()
        if formals is not rinterface.NULL:
            (symbol_mapping, 
             conflicts, 
             resolutions) = _map_symbols(formals.names,
                                         translation = prm_translate,
                                         symbol_r2python = symbol_r2python,
                                         symbol_check_after = symbol_check_after)

            msg_prefix = 'Conflict when converting R symbols'+\
                ' in the function\'s signature:\n- '
            exception = ValueError
            _fix_map_symbols(symbol_mapping,
                             conflicts,
                             on_conflict,
                             msg_prefix,
                             exception)
            symbol_mapping.update(resolutions)
            reserved_pynames = set(dir(self))
            
            prm_translate.update((k, v[0]) for k, v in symbol_mapping.items())
        self._prm_translate = prm_translate
        if hasattr(sexp, '__rname__'):
            self.__rname__ = sexp.__rname__

    def __call__(self, *args, **kwargs):
        prm_translate = self._prm_translate
        for k in tuple(kwargs.keys()):
            r_k = prm_translate.get(k, None)
            if r_k is not None:
                v = kwargs.pop(k)
                kwargs[r_k] = v
        return super(SignatureTranslatedFunction, self).__call__(*args, **kwargs)

pattern_link = re.compile(r'\\link\{(.+?)\}')
pattern_code = re.compile(r'\\code\{(.+?)\}')
pattern_samp = re.compile(r'\\samp\{(.+?)\}')
class DocumentedSTFunction(SignatureTranslatedFunction):

    def __init__(self, sexp, init_prm_translate = None,
                 packagename = None):
        super(DocumentedSTFunction, 
              self).__init__(sexp, 
                             init_prm_translate = init_prm_translate)
        self.__rpackagename__ = packagename

    @docstring_property(__doc__)
    def __doc__(self):
        doc = ['Python representation of an R function.']
        description = help.docstring(self.__rpackagename__,
                                     self.__rname__,
                                     sections=['description'])
        doc.append(description)

        fm = _formals_fixed(self)
        names = fm.do_slot('names')
        doc.append(self.__rname__+'(')
        for key, val in self._prm_translate.items():
            if key == '___':
                description = '(was "..."). R ellipsis (any number of parameters)'
            else:
                description = _repr_argval(fm[names.index(val)])
            if description is None:
                doc.append('    %s,' % key)                
            else:
                doc.append('    %s = %s,' % (key, description))
        doc.extend((')', ''))
        package = help.Package(self.__rpackagename__)
        page = package.fetch(self.__rname__)
        for item in page.arguments():
            description = item.value
            description = description.replace('\n', '')
            description, count = pattern_link.subn(r'\1', description)
            description, count = pattern_code.subn(r'`\1`', description)
            description, count = pattern_samp.subn(r'`\1`', description)
            doc.append(' '.join((item.name, ': ', description, ',')))
            doc.append('')
        return os.linesep.join(doc)
