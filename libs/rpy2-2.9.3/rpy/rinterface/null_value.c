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
 * Copyright (C) 2008-2010 Laurent Gautier
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


/* --- NULL value --- */

PyDoc_STRVAR(RNULL_Type_doc,
	     "R NULL (singleton)."
	     );

static PyTypeObject RNULL_Type;

static PyObject*
RNULLType_tp_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  static PySexpObject *self = NULL;
  static char *kwlist[] = {0};

  if (! PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist)) {
    return NULL;
  }

  if (self == NULL) {
    self = (PySexpObject*)(Sexp_Type.tp_new(&RNULL_Type, Py_None, Py_None));
    if (self == NULL) {
      return NULL;
    }
  }
  Py_XINCREF(self);
  return (PyObject *)self;
}

static PyObject*
RNULLType_tp_init(PyObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = {0};
  if (! PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist)) {
    return NULL;
  }
  return 0;
}

static PyObject*
RNULLType_repr(PyObject *self)
{
  static PyObject* repr = NULL;
  if (repr == NULL) {
    repr = PyUnicode_FromString("rpy2.rinterface.NULL");
  }
  Py_XINCREF(repr);
  return repr;
}

static PyObject*
RNULLType_str(PyObject *self)
{
  static PyObject* repr = NULL;
  if (repr == NULL) {
    repr = PyUnicode_FromString("NULL");
  }
  Py_XINCREF(repr);
  return repr;
}

static int
RNULLType_nonzero(PyObject *self)
{
  return 0;
}


static PyNumberMethods RNULLType_as_number = {
  0,                       /* nb_add */
  0,                       /* nb_subtract */
  0,                       /* nb_multiply */
  0,                       /* nb_remainder */
  0,                    /* nb_divmod */
  0,                       /* nb_power */
  0,            /* nb_negative */
  0,            /* nb_positive */
  0,            /* nb_absolute */
  (inquiry)RNULLType_nonzero,          /* nb_nonzero */
  0,         /* nb_invert */
  0,                    /* nb_lshift */
  0,                    /* nb_rshift */
  0,                       /* nb_and */
  0,                       /* nb_xor */
  0,                        /* nb_or */
  0,            /* nb_int */
  NULL, /* reserved */
  0,          /* nb_float */
  /* added in release 2.0 */
  0,                      /* nb_inplace_add */
  0,                      /* nb_inplace_subtract */
  0,                      /* nb_inplace_multiply */
  0,                      /* nb_inplace_remainder */
  0,                      /* nb_inplace_power */
  0,                   /* nb_inplace_lshift */
  0,                   /* nb_inplace_rshift */
  0,                      /* nb_inplace_and */
  0,                      /* nb_inplace_xor */
  0,                       /* nb_inplace_or */
  /* added in release 2.2 */
  0,                  /* nb_floor_divide */
  0,                   /* nb_true_divide */
  0,                 /* nb_inplace_floor_divide */
  0,                  /* nb_inplace_true_divide */
  /* added in version 2.5 */
  0,          /* nb_index */
};


static PyTypeObject RNULL_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.RNULLType",       /*tp_name*/
        sizeof(PySexpObject),   /*tp_basicsize*/
        0,                      /*tp_itemsize*/
        /* methods */
        0, /*tp_dealloc*/
        0,                      /*tp_print*/
        0,                      /*tp_getattr*/
        0,                      /*tp_setattr*/
        0,                      /*tp_compare*/
        RNULLType_repr,                      /*tp_repr*/
        &RNULLType_as_number,                      /*tp_as_number*/
        0,                      /*tp_as_sequence*/
        0,                      /*tp_as_mapping*/
        0,                      /*tp_hash*/
        0,                      /*tp_call*/
        RNULLType_str,                      /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /*tp_flags*/
        RNULL_Type_doc,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,           /*tp_methods*/
        0,                      /*tp_members*/
        0,                      /*tp_getset*/
        &Sexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)RNULLType_tp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        RNULLType_tp_new,                      /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};


static PyObject*
RNULL_Type_New(int new)
{
  RPY_NA_NEW(RNULL_Type, RNULLType_tp_new)
}




/* Unbound marker value */

PyDoc_STRVAR(UnboundValue_Type_doc,
"Unbound marker (R_UnboundValue in R's C API)."
);

static PyTypeObject UnboundValue_Type;

static PyObject*
UnboundValueType_tp_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  static PySexpObject *self = NULL;
  static char *kwlist[] = {0};

  if (! PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist)) {
    return NULL;
  }

  if (self == NULL) {
    self = (PySexpObject*)(Sexp_Type.tp_new(&UnboundValue_Type, Py_None, Py_None));
    if (self == NULL) {
      return NULL;
    }
  }
  Py_XINCREF(self);
  return (PyObject *)self;
}

static PyObject*
UnboundValueType_tp_init(PyObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = {0};
  if (! PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist)) {
    return NULL;
  }
  return 0;
}

static PyObject*
UnboundValueType_repr(PyObject *self)
{
  static PyObject* repr = NULL;
  if (repr == NULL) {
    repr = PyUnicode_FromString("rpy2.rinterface.UnboundValue");
  }
  Py_XINCREF(repr);
  return repr;
}

static PyObject*
UnboundValueType_str(PyObject *self)
{
  static PyObject* repr = NULL;
  if (repr == NULL) {
    repr = PyUnicode_FromString("UnboundValue");
  }
  Py_XINCREF(repr);
  return repr;
}

static PyTypeObject UnboundValue_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
#if (PY_VERSION_HEX < 0x03010000)
        PyObject_HEAD_INIT(NULL)
        0,                      /*ob_size*/
#else
	PyVarObject_HEAD_INIT(NULL, 0)
#endif
        "rpy2.rinterface.UnboundValueType",       /*tp_name*/
        sizeof(PySexpObject),   /*tp_basicsize*/
        0,                      /*tp_itemsize*/
        /* methods */
        0, /*tp_dealloc*/
        0,                      /*tp_print*/
        0,                      /*tp_getattr*/
        0,                      /*tp_setattr*/
        0,                      /*tp_compare*/
        UnboundValueType_repr,                      /*tp_repr*/
        0,                      /*tp_as_number*/
        0,                      /*tp_as_sequence*/
        0,                      /*tp_as_mapping*/
        0,                      /*tp_hash*/
        0,                      /*tp_call*/
        UnboundValueType_str,                      /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
        0,                      /*tp_as_buffer*/
#if (PY_VERSION_HEX < 0x03010000)
        Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_CHECKTYPES, /*tp_flags*/
#else
        Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /*tp_flags*/
#endif
        UnboundValue_Type_doc,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0, //NAInteger_methods,           /*tp_methods*/
        0,                      /*tp_members*/
        0,                      /*tp_getset*/
        &Sexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)UnboundValueType_tp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        UnboundValueType_tp_new,                      /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};


static PyObject*
UnboundValue_Type_New(int new)
{
  RPY_NA_NEW(UnboundValue_Type, UnboundValueType_tp_new)
}

