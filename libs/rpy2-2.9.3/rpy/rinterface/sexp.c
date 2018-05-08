/*
 ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * Copyright (C) 2008-2012 Laurent Gautier
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <Python.h>

#include "r_utils.h"
#include "embeddedr.h"
#include "sexp.h"


/* This is tp_clear() for Sexp objects. */
static void
Sexp_clear(PySexpObject *self)
{
  Rpy_ReleaseObject(self->sObj->sexp);
}


static void
Sexp_dealloc(PySexpObject *self)
{
  Sexp_clear(self);
  (Py_TYPE(self)->tp_free)((PyObject *)self);
  /* PyObject_Del(self); */
}


static PyObject*
Sexp_repr(PyObject *self)
{
  SEXP sexp = RPY_SEXP((PySexpObject *)self);
  /* if (! sexp) {
   *  PyErr_Format(PyExc_ValueError, "NULL SEXP.");
   *  return NULL;
   *}
   */
  return PyUnicode_FromFormat("<%s - Python:\%p / R:\%p>",
			      self->ob_type->tp_name,
			      self,
			      sexp);
}


static PyObject*
Sexp_typeof_get(PyObject *self)
{
  PySexpObject *pso = (PySexpObject*)self;
  SEXP sexp = RPY_SEXP(pso);
  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;
  }
  return PyLong_FromLong((long)TYPEOF(sexp));
}
PyDoc_STRVAR(Sexp_typeof_doc,
             "R internal SEXPREC type.");


PyDoc_STRVAR(Sexp_list_attr_doc,
             "Returns the list of attribute names.");
PyObject*
Sexp_list_attr(PyObject *self)
{
  SEXP sexp = RPY_SEXP(((PySexpObject*)self));
  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;
  }
  SEXP res_R;
  PROTECT(res_R = rpy2_list_attr(sexp));
  PyObject *res = (PyObject *)newPySexpObject(res_R);
  UNPROTECT(1);
  return res;
}

static PyObject*
Sexp_do_slot(PyObject *self, PyObject *name)
{
  SEXP sexp = RPY_SEXP(((PySexpObject*)self));
  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;
  }
  if (! PyUnicode_Check(name)) {
    PyErr_SetString(PyExc_TypeError, "The name must be a string.");
    return NULL;
  }
  if (PyUnicode_GET_LENGTH(name) == 0) {
    PyErr_SetString(PyExc_ValueError, "The name cannot be an empty string");
    return NULL;
  }

  PyObject *pybytes = PyUnicode_AsLatin1String(name);
  char *name_str = PyBytes_AsString(pybytes);
  if (! R_has_slot(sexp, install(name_str))) {
    PyErr_SetString(PyExc_LookupError, "The object has no such attribute.");
    Py_DECREF(pybytes);
    return NULL;
  }
  SEXP res_R = GET_SLOT(sexp, install(name_str));
    Py_DECREF(pybytes);
  PyObject *res = (PyObject *)newPySexpObject(res_R);
  return res;
}
PyDoc_STRVAR(Sexp_do_slot_doc,
             "Returns the attribute/slot for an R object.\n"
             " The name of the slot (a string) is the only parameter for\n"
             "the method.\n"
             ":param name: string\n"
             ":rtype: instance of type or subtype :class:`rpy2.rinterface.Sexp`");

static PyObject*
Sexp_do_slot_assign(PyObject *self, PyObject *args)
{

  SEXP sexp = RPY_SEXP(((PySexpObject*)self));
  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;;
  }

  char *name_str;
  PyObject *name, *value;
  if (! PyArg_ParseTuple(args, "UO", 
                         &name,
                         &value)) {
    return NULL;
  }
  if (PyUnicode_GetLength(name) == 0) {
    PyErr_SetString(PyExc_ValueError, "The name cannot be an empty string");
    return NULL;
  }
  PyObject *pybytes = PyUnicode_AsLatin1String(name);
  name_str = PyBytes_AsString(pybytes);
  Py_DECREF(pybytes);

  if (! PyObject_IsInstance(value, 
                          (PyObject*)&Sexp_Type)) {
      PyErr_Format(PyExc_ValueError, "Value must be an instance of Sexp.");
      return NULL;
  }

  SEXP value_sexp = RPY_SEXP((PySexpObject *)value);
  if (! value_sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;;
  }

  SET_SLOT(sexp, install(name_str), value_sexp);
  Py_INCREF(Py_None);
  return Py_None;
}
PyDoc_STRVAR(Sexp_do_slot_assign_doc,
             "Set the attribute/slot for an R object.\n"
             "\n"
             ":param name: string\n"
             ":param value: instance of :class:`rpy2.rinterface.Sexp`");

static PyObject*
Sexp_named_get(PyObject *self)
{
  SEXP sexp = RPY_SEXP(((PySexpObject*)self));
  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;;
  }
  unsigned int res = NAMED(sexp);
  return PyLong_FromLong((long)res);
}
PyDoc_STRVAR(Sexp_named_doc,
"Integer code for the R object reference-pseudo counting.\n\
This method corresponds to the macro NAMED.\n\
See the R-extensions manual for further details.");


/* Get the underlying R object exposed by rpy2 as a Python capsule.
   This is needed to overcome the pass-by-value (pass-by-need) paradigm
   in R and provide the appearance of pass-by-reference from the Python
   side.
 */
static PyObject*
Sexp_sexp_get(PyObject *self, void *closure)
{
  PySexpObject* rpyobj = (PySexpObject*)self;

  if (! RPY_SEXP(rpyobj)) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;;
  }

  PyObject *key = PyLong_FromVoidPtr((void *)(rpyobj->sObj->sexp));
  PyObject *capsule = PyDict_GetItem(Rpy_R_Precious, key);
  if (capsule == NULL) {
    printf("Error: Could not get the capsule for the SEXP. This means trouble.\n");
    return NULL;
  }
  Py_DECREF(key);
  /* capsule is a borrowed reference: INCREF */
  Py_INCREF(capsule);
  return capsule;
}

/* Assign a new underlying R object to the Python representation */
static int
Sexp_sexp_set(PyObject *self, PyObject *obj, void *closure)
{

  if (! PyCapsule_CheckExact(obj)) {
    PyErr_SetString(PyExc_TypeError, "The value must be a Capsule");
    return -1;
  }

  SexpObject *sexpobj_new = (SexpObject *)(PyCapsule_GetPointer(obj,
								"rpy2.rinterface._rinterface.SEXPOBJ_C_API"));
  
  if (sexpobj_new == NULL) {
    PyErr_SetString(PyExc_TypeError, 
		    "The value must be a CObject or a Capsule of name 'rpy2.rinterface._rinterface.SEXPOBJ_C_API'.");
    return -1;
  }

  SexpObject *sexpobj_orig = ((PySexpObject*)self)->sObj;
  #ifdef RPY_DEBUG_COBJECT
  printf("Setting %p (count: %i) to %p (count: %i)\n", 
         sexpobj_orig, (int)sexpobj_orig->pycount,
         sexpobj_new, (int)sexpobj_new->pycount);
  #endif

  if ( (sexpobj_orig->sexp != R_NilValue) &
       (TYPEOF(sexpobj_orig->sexp) != TYPEOF(sexpobj_new->sexp))
      ) {
    PyErr_Format(PyExc_ValueError, 
                 "Mismatch in SEXP type (as returned by typeof)");
    return -1;
  }

  SEXP sexp = sexpobj_new->sexp;
  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return -1;
  }

  return Rpy_ReplaceSexp((PySexpObject *)self, sexp);

}
PyDoc_STRVAR(Sexp_sexp_doc,
             "Opaque C pointer to the underlying R object");

static PyObject*
Sexp_rclass_get(PyObject *self, void *closure)
{
  SEXP sexp = RPY_SEXP(((PySexpObject*)self));
  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;;
  }

  /* SEXP res_R = R_data_class(sexp, TRUE);*/
  /* R_data_class is not exported, although R's own
   package "methods" needs it as part of the API.
   We are getting the R class by ourselves. This
   is problematic since we are now exposed to changes
   in the behaviour of R_data_class. */
  SEXP res_R = getAttrib(sexp, R_ClassSymbol);
  int nclasses = length(res_R);
  if (nclasses == 0) {
    /* if no explicit class, R will still consider the presence
     of dimensions, then the "TYPEOF" */
    SEXP sexp_dim = getAttrib(sexp, R_DimSymbol);
    int nd = length(sexp_dim);
    if(nd > 0) {
      if(nd == 2)
	res_R = mkChar("matrix");
      else
	res_R = mkChar("array");
    } else {
      SEXPTYPE t = TYPEOF(sexp);
      switch(t) {
      case CLOSXP:
      case SPECIALSXP:
      case BUILTINSXP:
	res_R = mkChar("function");
	break;
      case REALSXP:
	res_R = mkChar("numeric");
	break;
      case SYMSXP:
	res_R = mkChar("name");
	break;
      case LANGSXP:
	/* res_R = lang2str(sexp, t);*/
	/* lang2str is not part of the R API, yadayadayada....*/
	res_R = rpy2_lang2str(sexp, t);
	break;
      default:
	res_R = Rf_type2str(t);
      }
    } 
  } else {
    res_R = asChar(res_R);
  }
  PROTECT(res_R);
  SEXP class_Rstring = ScalarString(res_R);
  UNPROTECT(1);
  PyObject *res = (PyObject *)newPySexpObject(class_Rstring);
  return res;
}

/* Return -1 on failure, with an exception set. */
static int
Sexp_rclass_set(PyObject *self, PyObject *value, void *closure)
{
  SEXP sexp = RPY_SEXP(((PySexpObject*)self));
  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return -1;
  }


  if (! PyObject_IsInstance(value, 
			    (PyObject*)&Sexp_Type)) {
    PyErr_Format(PyExc_ValueError, "Value must be a Sexp.");
    return -1;
  }
  SEXP sexp_class = RPY_SEXP((PySexpObject*)value);
  SET_CLASS(sexp, sexp_class);
  return 0;
}
PyDoc_STRVAR(Sexp_rclass_doc,
             "R class name (and in R the class is an attribute and can be set).");



static PyObject*
Sexp_rid_get(PyObject *self)
{
  SEXP sexp = RPY_SEXP(((PySexpObject*)self));
  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;;
  }
  PyObject *res = PyLong_FromVoidPtr((void *)sexp);
  return res;
}
PyDoc_STRVAR(Sexp_rid_doc,
             "ID for the associated R object (Hint: that's a memory address)");


static PyObject*
Sexp_refcount_get(PyObject *self)
{
  PySexpObject* rpyobj = (PySexpObject*)self;

  if (! RPY_SEXP(rpyobj)) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;
  }
  PyObject *res = PyLong_FromLong((long)(rpyobj->sObj->pycount));
  return res;
}
PyDoc_STRVAR(Sexp_refcount_doc,
             "Reference counter for the underlying R object");


static PyObject*
Sexp_rsame(PyObject *self, PyObject *other)
{
  
  if (! PyObject_IsInstance(other, 
                            (PyObject*)&Sexp_Type)) {
    PyErr_Format(PyExc_ValueError, 
                 "Can only compare Sexp objects.");
    return NULL;
  }
  
  SEXP sexp_self = RPY_SEXP(((PySexpObject*)self));
  if (! sexp_self) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;;
  }
  
  SEXP sexp_other = RPY_SEXP(((PySexpObject*)other));
  if (! sexp_other) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;;
  }
  
  long same = (sexp_self == sexp_other);
  return PyBool_FromLong(same);
}
PyDoc_STRVAR(Sexp_rsame_doc,
             "Is the given object representing the same underlying R object as the instance.");

static PyObject*
Sexp_duplicate(PyObject *self, PyObject *kwargs)
{
  SEXP sexp_self, sexp_copy;
  PyObject *res;
  
  sexp_self = RPY_SEXP((PySexpObject*)self);
  if (! sexp_self) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;;
  }
  PROTECT(sexp_copy = Rf_duplicate(sexp_self));
  res = (PyObject *) newPySexpObject(sexp_copy);
  UNPROTECT(1);
  return res;
}
PyDoc_STRVAR(Sexp_duplicate_doc,
             "Makes a copy of the underlying Sexp object, and returns it.");

static PyObject*
Sexp___getstate__(PyObject *self)
{

  PyObject *res_string;

  SEXP sexp = RPY_SEXP((PySexpObject *)self);
  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;
  }

  SEXP sexp_ser;
  PROTECT(sexp_ser = rpy2_serialize(sexp, R_GlobalEnv));
  if (TYPEOF(sexp_ser) != RAWSXP) {
    UNPROTECT(1);
    PyErr_Format(PyExc_RuntimeError, 
                 "R's serialize did not return a raw vector.");
    return NULL;
  }
  /* PyByteArray is only available with Python >= 2.6 */
  /* res = PyByteArray_FromStringAndSize(sexp_ser, len); */

  res_string = PyBytes_FromStringAndSize((char *)RAW_POINTER(sexp_ser), 
					 (Py_ssize_t)LENGTH(sexp_ser));
  if (res_string == NULL) {
    UNPROTECT(1);
    PyErr_Format(PyExc_RuntimeError, 
                 "Error while trying to build Python bytes"
		 " from serialized R object.");
    return NULL;
  }

  UNPROTECT(1);
  return res_string;
  /* Py_DECREF(res_string); */
  /* Py_INCREF(Py_None); */
  /* return Py_None; */
}

PyDoc_STRVAR(Sexp___getstate___doc,
             "Returns a serialized object for the underlying R object");


static PyObject*
Sexp___setstate__(PyObject *self, PyObject *state)
{

  char *raw;
  Py_ssize_t raw_size;
  int status;
  
  status = PyBytes_AsStringAndSize(state, &raw, &raw_size);
  if (status == -1) {
    return NULL;
  }
  
  SEXP sexp;
  PROTECT(sexp = _rpy2_unserialize_from_char_and_size(raw, raw_size));  
  status = Rpy_ReplaceSexp((PySexpObject *)self,
			   sexp);
  UNPROTECT(1);
  if (status == -1) {
    /* TODO: raise an exception */
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;

}

PyDoc_STRVAR(Sexp___setstate___doc,
             "set the state of an rpy2 object.");

static SEXP
_rpy2_unserialize_from_char_and_size(char *raw, Py_ssize_t size)
{

  /* TODO: handle the case where "size" is larger than the possible
   *   R vector size.
  */
  
  /* Not the most memory-efficient; an other option would
  * be to create a dummy RAW and rebind "raw" as its content
  * (wich seems clearly off the charts).
  */
  SEXP raw_sexp, sexp_ser;
  PROTECT(raw_sexp = NEW_RAW((int)size));

  /*FIXME: use of the memcpy seems to point in the direction of
  * using the option mentioned above anyway. */
  Py_ssize_t raw_i;
  for (raw_i = 0; raw_i < size; raw_i++) {
    RAW_POINTER(raw_sexp)[raw_i] = raw[raw_i];
  }
  PROTECT(sexp_ser = rpy2_unserialize(raw_sexp, R_GlobalEnv));
  /* TODO: handle error */
  UNPROTECT(2);
  return sexp_ser;
}


static PyObject*
EmbeddedR_unserialize(PyObject* self, PyObject* args)
{
  PyObject *res;

  if (! (rpy_has_status(RPY_R_INITIALIZED))) {
    PyErr_Format(PyExc_RuntimeError, 
                 "R cannot evaluate code before being initialized.");
    return NULL;
  }
  

  char *raw;
  Py_ssize_t raw_size;
  int rtype;
  if (! PyArg_ParseTuple(args, "s#i",
                         &raw, &raw_size,
                         &rtype)) {
    return NULL;
  }

  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return NULL;
  }
  embeddedR_setlock();
  SEXP sexp_ser;
  PROTECT(sexp_ser = _rpy2_unserialize_from_char_and_size(raw, raw_size));

  if (TYPEOF(sexp_ser) != rtype) {
    UNPROTECT(1);
    PyErr_Format(PyExc_ValueError, 
                 "Mismatch between the serialized object"
                 " and the expected R type"
                 " (expected %i but got %i)",
		 rtype, TYPEOF(sexp_ser));
    return NULL;
  }
  res = (PyObject*)newPySexpObject(sexp_ser);
  
  UNPROTECT(1);
  embeddedR_freelock();
  return res;
}



static PyMethodDef Sexp_methods[] = {
  {"list_attrs", (PyCFunction)Sexp_list_attr, METH_NOARGS,
   Sexp_list_attr_doc},
  {"do_slot", (PyCFunction)Sexp_do_slot, METH_O,
   Sexp_do_slot_doc},
  {"do_slot_assign", (PyCFunction)Sexp_do_slot_assign, METH_VARARGS,
   Sexp_do_slot_assign_doc},
  {"rsame", (PyCFunction)Sexp_rsame, METH_O,
   Sexp_rsame_doc},
  {"__deepcopy__", (PyCFunction)Sexp_duplicate, METH_VARARGS | METH_KEYWORDS,
   Sexp_duplicate_doc},
  {"__getstate__", (PyCFunction)Sexp___getstate__, METH_NOARGS,
   Sexp___getstate___doc},
  {"__setstate__", (PyCFunction)Sexp___setstate__, METH_O,
   Sexp___setstate___doc},
  {NULL, NULL}          /* sentinel */
};


static PyGetSetDef Sexp_getsets[] = {
  {"named", 
   (getter)Sexp_named_get,
   (setter)0,
   Sexp_named_doc},
  {"typeof", 
   (getter)Sexp_typeof_get,
   (setter)0,
   Sexp_typeof_doc},
  {"rclass", 
   (getter)Sexp_rclass_get,
   (setter)Sexp_rclass_set,
   Sexp_rclass_doc},
  {"rid", 
   (getter)Sexp_rid_get,
   (setter)0,
   Sexp_rid_doc},
  {"__sexp__",
   (getter)Sexp_sexp_get,
   (setter)Sexp_sexp_set,
   Sexp_sexp_doc},
  {"__sexp_refcount__",
   (getter)Sexp_refcount_get,
   (setter)0,
   Sexp_refcount_doc},
  {NULL, NULL, NULL, NULL}          /* sentinel */
};


static PyObject*
Sexp_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{

  PySexpObject *self = NULL;
  /* unsigned short int rpy_only = 1; */
  #ifdef RPY_VERBOSE
  printf("new '%s' object @...\n", type->tp_name);
  #endif 

  /* self = (PySexpObject *)PyObject_New(PySexpObject, type); */
  self = (PySexpObject *)type->tp_alloc(type, 0);
  #ifdef RPY_VERBOSE
  printf("  Python:%p / R:%p (R_NilValue) ...\n", self, R_NilValue);
  #endif 

  if (! self)
    PyErr_NoMemory();
  self->sObj = Rpy_PreserveObject(R_NilValue);
  if (self->sObj == NULL) {
    printf("Error in Sexp_new. This is not looking good...\n");
  }

  #ifdef RPY_VERBOSE
  printf("done.\n");
  #endif 

  return (PyObject *)self;

}


static int
Sexp_init(PyObject *self, PyObject *args, PyObject *kwds)
{
#ifdef RPY_VERBOSE
  printf("Python:%p / R:%p - Sexp initializing...\n", 
         self, RPY_SEXP((PySexpObject *)self));
#endif 

  PyObject *sourceObject;

  PyObject *copy = Py_True;
  int sexptype = -1;

  static char *kwlist[] = {"sexp", "sexptype", NULL};
  /* FIXME: handle the copy argument */

  /* the "sexptype" is as a quick hack to make calls from
   the constructor of SexpVector */
  if (! PyArg_ParseTupleAndKeywords(args, kwds, "O|i", 
                                    kwlist,
                                    &sourceObject,
                                    &sexptype)) {
    return -1;
  }

  if (! PyObject_IsInstance(sourceObject, 
                            (PyObject*)&Sexp_Type)) {
    PyErr_Format(PyExc_ValueError, 
                 "Can only instanciate from Sexp objects.");
    return -1;
  }

  /* Since sourceObject is a Sexp_Type, the R object is
   already tracked. */

  int status = Rpy_ReplaceSexp(((PySexpObject *)self),
			       ((PySexpObject *)sourceObject)->sObj->sexp);
  /* int returnvalu = _replace_Sexp((PySexpObject *)sourceObject, */
  /* 				 (PySexpObject *)self)); */

  if (status == -1) {
    return -1;
  }

  
  //RPY_INCREF((PySexpObject *)self);
#ifdef RPY_VERBOSE
  printf("Python: %p / R: %p - sexp count is now %i.\n", 
	 (PySexpObject *)self,
	 RPY_SEXP((PySexpObject *)self),
	 RPY_COUNT((PySexpObject *)self));
#endif 


#ifdef RPY_VERBOSE
  printf("done.\n");
#endif 
  /* SET_NAMED(RPY_SEXP((PySexpObject *)self), (unsigned int)2); */
  return 0;
}


/*
 * Generic Sexp_Type. It represents SEXP objects at large.
 */
static PyTypeObject Sexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.Sexp",      /*tp_name*/
        sizeof(PySexpObject),   /*tp_basicsize*/
        0,                      /*tp_itemsize*/
        /* methods */
        (destructor)Sexp_dealloc, /*tp_dealloc*/
        0,                      /*tp_print*/
        0,                      /*tp_getattr*/
        0,                      /*tp_setattr*/
        0,                      /*tp_compare*/
        Sexp_repr,              /*tp_repr*/
        0,                      /*tp_as_number*/
        0,                      /*tp_as_sequence*/
        0,                      /*tp_as_mapping*/
        0,                      /*tp_hash*/
        0,                      /*tp_call*/
        0,                      /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,     /*tp_flags*/
        0,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        (inquiry)Sexp_clear,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        Sexp_methods,           /*tp_methods*/
        0,                      /*tp_members*/
        Sexp_getsets,            /*tp_getset*/
        0,                      /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)Sexp_init,    /*tp_init*/
        0,                      /*tp_alloc*/
        Sexp_new,               /*tp_new*/
        0,                      /*tp_free*/
        0,                      /*tp_is_gc*/
};


