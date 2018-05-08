import sys
import six
if sys.version_info[0] < 3 or \
   (sys.version_info[0] == 3 and sys.version_info[1] < 4):
    class SimpleNamespace:
        def __init__(self, **kwargs):
            self.__dict__.update(kwargs)
        def __repr__(self):
            keys = sorted(self.__dict__)
            items = ("{}={!r}".format(k, self.__dict__[k]) for k in keys)
            return "{}({})".format(type(self).__name__, ", ".join(items))
        def __eq__(self, other):
            return self.__dict__ == other.__dict__
else:
    from types import SimpleNamespace
from rpy2.robjects.robject import RObjectMixin
import rpy2.rinterface as rinterface
from rpy2.rinterface import StrSexpVector
from rpy2.robjects import help as rhelp
from rpy2.robjects import conversion

getmethod = rinterface.baseenv.get("getMethod")

require = rinterface.baseenv.get('require')
require(StrSexpVector(('methods', )),
        quiet = rinterface.BoolSexpVector((True, )))


class RS4(RObjectMixin, rinterface.SexpS4):
    """ Python representation of an R instance of class 'S4'. """

    def slotnames(self):
        """ Return the 'slots' defined for this object """
        return methods_env['slotNames'](self)
    
    def do_slot(self, name):
        return conversion.ri2ro(super(RS4, self).do_slot(name))

    @staticmethod
    def isclass(name):
        """ Return whether the given name is a defined class. """
        name = conversion.py2ri(name)
        return methods_env['isClass'](name)[0]

    def validobject(self, test = False, complete = False):
        """ Return whether the instance is 'valid' for its class. """
        test = conversion.py2ri(test)
        complete = conversion.py2ri(complete)
        return methods_env['validObject'](self, test = test,
                                          complete = complete)[0]

class ClassRepresentation(RS4):
    """ Definition of an R S4 class """
    slots = property(lambda x: [y[0] for y in x.do_slot('slots')],
                     None, None,
                     "Slots (attributes) for the class")
    
    basenames = property(lambda x: [y[0] for y in x.do_slot('contains')],
                         None, None,
                         "Parent classes")
    contains = basenames

    isabstract = property(lambda x: x.do_slot('virtual')[0],
                          None, None,
                          "Is the class an abstract class ?")
    virtual = isabstract

    packagename = property(lambda x: x.do_slot('package')[0],
                           None, None,
                           "R package in which the class is defined")
    package = packagename

    classname = property(lambda x: x.do_slot('className')[0],
                         None, None,
                         "Name of the R class")


def getclassdef(cls_name, cls_packagename):
    cls_def = methods_env['getClassDef'](StrSexpVector((cls_name,)),
                                         StrSexpVector((cls_packagename, )))
    cls_def = ClassRepresentation(cls_def)
    cls_def.__rname__ = cls_name
    return cls_def

class RS4_Type(type):
    def __new__(mcs, name, bases, cls_dict):

        try:
            cls_rname = cls_dict['__rname__']
        except KeyError as ke:
            cls_rname = name

        try:
            accessors = cls_dict['__accessors__']
        except KeyError as ke:
            accessors = []
            
        for rname, where, \
                python_name, as_property, \
                docstring in accessors:

            if where is None:
                where = rinterface.globalenv
            else:
                where = "package:" + str(where)
                where = StrSexpVector((where, ))

            if python_name is None:
                python_name = rname
                
            signature = StrSexpVector((cls_rname, ))            
            r_meth = getmethod(StrSexpVector((rname, )), 
                               signature = signature,
                               where = where)
            r_meth = conversion.ri2ro(r_meth)
            if as_property:
                cls_dict[python_name] = property(r_meth, None, None,
                                                 doc = docstring)
            else:
                cls_dict[python_name] =  lambda self: r_meth(self)
                
        return type.__new__(mcs, name, bases, cls_dict)

# playground to experiment with more metaclass-level automation

class RS4Auto_Type(type):
    """ This type (metaclass) takes an R S4 class
    and create a Python class out of it,
    copying the R documention page into the Python docstring.
    A class with this metaclass has the following optional
    attributes: __rname__, __rpackagename__, __attr__translation,
    __meth_translation__.
    """
    def __new__(mcs, name, bases, cls_dict):
        try:
            cls_rname = cls_dict['__rname__']
        except KeyError as ke:
            cls_rname = name

        try:
            cls_rpackagename = cls_dict['__rpackagename__']
        except KeyError as ke:
            cls_rpackagename = None

        try:
            cls_attr_translation = cls_dict['__attr_translation__']
        except KeyError as ke:
            cls_attr_translation = {}
        try:
            cls_meth_translation = cls_dict['__meth_translation__']
        except KeyError as ke:
            cls_meth_translation = {}

        cls_def = getclassdef(cls_rname, cls_rpackagename)
    
        # documentation / help
        if cls_rpackagename is None:
            cls_dict['__doc__'] = "Undocumented class from the R workspace."
        else:
            pack_help = rhelp.Package(cls_rpackagename)
            page_help = None
            try:
                #R's classes are sometimes documented with a prefix 'class.'
                page_help = pack_help.fetch(cls_def.__rname__ + "-class")
            except rhelp.HelpNotFoundError as hnf:
                pass
            if page_help is None:
                try:
                    page_help = pack_help.fetch(cls_def.__rname__)
                except rhelp.HelpNotFoundError as hnf:
                    pass
            if page_help is None:
                cls_dict['__doc__'] = 'Unable to fetch R documentation for the class'
            else:
                cls_dict['__doc__'] = ''.join(page_help.to_docstring())
        
        for slt_name in cls_def.slots:
            #FIXME: sanity check on the slot name
            try:
                slt_name = cls_attr_translation[slt_name]
            except KeyError as ke:
                # no translation: abort
                pass

            #FIXME: isolate the slot documentation and have it here
            cls_dict[slt_name] = property(lambda self: self.do_slot(slt_name),
                                          None, None,
                                          None)

        # Now tackle the methods
        all_generics = methods_env['getGenerics']()
        findmethods = methods_env['findMethods']

        # does not seem elegant, but there is probably nothing else to do
        # than loop across all generics
        r_cls_rname = StrSexpVector((cls_rname, ))
        for funcname in all_generics:
            all_methods = findmethods(StrSexpVector((funcname, )), 
                                      classes = r_cls_rname)

            # skip if no methods (issue #301). R's findMethods() result
            # does not have an attribute "names" if of length zero.
            if len(all_methods) == 0:
                continue
            # all_methods contains all method/signature pairs
            # having the class we are considering somewhere in the signature
            # (the R/S4 systems allows multiple dispatch)
            for name, meth in zip(all_methods.do_slot("names"), all_methods):
                # R/S4 is storing each method/signature as a string, 
                # with the argument type separated by the character '#'
                # We will re-use that name for the Python name
                # (no multiple dispatch in python, the method name
                # will not be enough), replacing the '#'s with '__'s.
                signature = name.split("#")
                meth_name = '__'.join(signature)
                # function names ending with '<-' indicate that the function
                # is a setter of some sort. We reflect that by adding a 'set_'
                # prefix to the Python name (and of course remove the suffix '<-').
                if funcname.endswith('<-'):
                    meth_name = 'set_' + funcname[:-2] + '__' + meth_name
                else:
                    meth_name = funcname + '__' + meth_name
                # finally replace remaining '.'s in the Python name with '_'s
                meth_name = meth_name.replace('.', '_')
                
            #FIXME: sanity check on the function name
                try:
                    meth_name = cls_meth_translation[meth_name]
                except KeyError as ke:
                    # no translation: abort
                    pass

            #FIXME: isolate the slot documentation and have it here
                
                if meth_name in cls_dict:
                    raise Error("Duplicated attribute/method name.")
                cls_dict[meth_name] = meth

        return type.__new__(mcs, name, bases, cls_dict)


def set_accessors(cls, cls_name, where, acs):
    # set accessors (to be abandonned for the metaclass above ?)

    if where is None:
        where = rinterface.globalenv
    else:
        where = "package:" + str(where)
        where = StrSexpVector((where, ))

    for r_name, python_name, as_property, docstring in acs:
        if python_name is None:
            python_name = r_name
        r_meth = getmethod(StrSexpVector((r_name, )), 
                           signature = StrSexpVector((cls_name, )),
                           where = where)
        r_meth = conversion.ri2ro(r_meth)
        if as_property:
            setattr(cls, python_name, property(r_meth, None, None))
        else:
            setattr(cls, python_name, lambda self: r_meth(self))

def get_classnames(packname):
    res = methods_env['getClasses'](where = StrSexpVector(("package:%s" %packname, )))
    return tuple(res)

# Namespace to store the definition of RS4 classes
rs4classes = SimpleNamespace()

def _getclass(rclsname):
    if hasattr(rs4classes, rclsname):
        rcls = getattr(rs4classes, rclsname)
    else:
        # dynamically create a class
        rcls = type(rclsname, 
                    (RS4, ), 
                    dict())
        setattr(rs4classes,
                rclsname,
                rcls)
    return rcls

def rs4instance_factory(robj):
    """
    Return an RS4 objects (R objects in the 'S4' class system)
    as a Python object of type inheriting from `robjects.methods.RS4`.

    The types are located in the namespace `robjects.methods.rs4classes`,
    and a dummy type is dynamically created whenever necessary.
    """
    clslist = None
    if len(robj.rclass) > 1:
        raise ValueError('Currently unable to handle more than one class per object')
    for rclsname in robj.rclass:
        rcls = _getclass(rclsname)
        return rcls(robj)
    if clslist is None:
        return robj

methods_env = rinterface.baseenv.get('as.environment')(StrSexpVector(('package:methods', )))


