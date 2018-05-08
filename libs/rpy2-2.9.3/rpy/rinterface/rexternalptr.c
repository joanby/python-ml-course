/*
 ***** BEGIN LICENSE BLOCK *****
 * Version: GPLv2+
 *
 * 
 * Copyright (C) 2008-2016 Laurent Gautier
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * ***** END LICENSE BLOCK ***** */

/* 
 * As usual with rpy2, we have a Python objects that exposes an R object.
 * In this file the type is ExtPtrSexp and the R object is an
 * "external pointer" object that points to a 
 * Python object. This allows us to pass around a Python object within
 * the R side of rpy2.
 *
 * 
*/


/* Finalizer for R external pointers that are arbitrary Python objects */
static void
R_PyObject_decref(SEXP s)
{
  PyObject* pyo = (PyObject*)R_ExternalPtrAddr(s);
  if (pyo) {
    Py_DECREF(pyo);
    R_ClearExternalPtr(s);
  }
}


PyDoc_STRVAR(ExtPtrSexp_Type_doc,
	     "R object that is an 'external pointer',"
	     " a pointer to a data structure implemented at the C level.\n"
	     "SexpExtPtr(extref, tag = None, protected = None)");

/* PyDoc_STRVAR(ExtPtrSexp___init___doc, */
/* 	     "Construct an external pointer. " */
/* 	     ); */

static int
ExtPtrSexp_init(PySexpObject *self, PyObject *args, PyObject *kwds)
{
#ifdef RPY_VERBOSE
  printf("Python:%p / R:%p - ExtPtrSexp initializing...\n", 
         self, RPY_SEXP((PySexpObject *)self));
#endif
  if (! RPY_SEXP(self)) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return -1;
  }

  PyObject *pyextptr = Py_None;
  PyObject *pytag = Py_None;
  PyObject *pyprotected = Py_None;
  static char *kwlist[] = {"extptr", "tag", "protected", NULL};
  if (! PyArg_ParseTupleAndKeywords(args, kwds, "O|O!O!", 
                                    kwlist,
                                    &pyextptr,
                                    &Sexp_Type, &pytag,
                                    &Sexp_Type, &pyprotected)) {
    return -1;
  }
  
  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return -1;
  }
  embeddedR_setlock();

  /*FIXME: twist here - MakeExternalPtr will "preserve" the tag */
  /* but the tag is already preserved (when exposed as a Python object) */
  /* R_ReleaseObject(pytag->sObj->sexp); */
  SEXP rtag, rprotected, rres;
  if (pytag == Py_None) {
    rtag = R_NilValue;
  } else {
    rtag = RPY_SEXP((PySexpObject *)pytag);
  }
  if (pyprotected == Py_None) {
    rprotected = R_NilValue;
  } else {
    rprotected = RPY_SEXP((PySexpObject *)pyprotected);
  }
  /* The Python object pointed to by `pyextptr` will have its reference counter
  *  incremented by one as it will be wrapped in an R "external pointer" object.
  *  The destructor for the that R "external pointer" will take care of
  *  decrementing the counter. */
  Py_INCREF(pyextptr);
  rres  = R_MakeExternalPtr(pyextptr, rtag, rprotected);
  PROTECT(rres);
  /* Register the destructor */
  /* The extended form of the registration of a finalizer is sometimes causing
   * a segfault when the Python interpreter is exiting (issue #331). It is
   * unclear what is causing this (race condition between R and Python's
   * respective garbage collectors ?), but switching to the short form of the
   * registration of a finalizer appears to solve the issue without any
   * visible negative effect (no observed leak).
   *
   * For reference, the long form was:
   * R_RegisterCFinalizerEx(rres, (R_CFinalizer_t)R_PyObject_decref, TRUE); 
   */
  R_RegisterCFinalizer(rres, (R_CFinalizer_t)R_PyObject_decref);
  UNPROTECT(1);
  if (Rpy_ReplaceSexp((PySexpObject *)self, rres) == -1) {
      embeddedR_freelock();
      return -1;
  }

#ifdef RPY_VERBOSE
  printf("done.\n");
#endif 
  embeddedR_freelock();
  return 0;
}


PyDoc_STRVAR(ExtPtrSexp___address___doc,
	     "The C handle to external data as a PyCObject."
	     );

static PyObject*
ExtPtrSexp_address(PySexpObject *self)
{
  if (! RPY_SEXP(self)) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;
  }
  embeddedR_setlock();
#if (PY_VERSION_HEX < 0x02070000) 
  PyObject *res = PyCObject_FromVoidPtr(R_ExternalPtrAddr(self->sObj->sexp), 
                                        NULL);
#else
  PyObject *res = PyCapsule_New(R_ExternalPtrAddr(self->sObj->sexp),
				"rpy2.rinterface._rinterface.SEXPOBJ_C_API",
				NULL);
#endif
  embeddedR_freelock();
  return res;
}


PyDoc_STRVAR(ExtPtrSexp___tag___doc,
	     "The R tag associated with the external pointer"
	     );

static PySexpObject*
ExtPtrSexp_tag(PySexpObject *self)
{
  if (! RPY_SEXP(self)) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;
  }
  embeddedR_setlock();
  SEXP rtag = R_ExternalPtrTag(self->sObj->sexp);
  PySexpObject *res = newPySexpObject(rtag);
  embeddedR_freelock();
  return res;
}

PyDoc_STRVAR(ExtPtrSexp___protected___doc,
	     "The R 'protected' object associated with the external pointer"
	     );

static PySexpObject*
ExtPtrSexp_protected(PySexpObject *self)
{
  if (! RPY_SEXP(self)) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;
  }
  embeddedR_setlock();
  SEXP rtag = R_ExternalPtrProtected(self->sObj->sexp);
  PySexpObject *res = newPySexpObject(rtag);
  embeddedR_freelock();
  return res;
}

static PyGetSetDef ExtPtrSexp_getsets[] = {
  {"__address__", 
   (getter)ExtPtrSexp_address,
   (setter)0,
   ExtPtrSexp___address___doc},
  {"__tag__", 
   (getter)ExtPtrSexp_tag,
   (setter)0,
   ExtPtrSexp___tag___doc},
  {"__protected__", 
   (getter)ExtPtrSexp_protected,
   (setter)0,
   ExtPtrSexp___protected___doc},
{NULL, NULL, NULL, NULL} /* sentinel */
};

static PyTypeObject ExtPtrSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
#if (PY_VERSION_HEX < 0x03010000)
        PyObject_HEAD_INIT(NULL)
        0,                      /*ob_size*/
#else
	PyVarObject_HEAD_INIT(NULL, 0)
#endif
        "rpy2.rinterface.SexpExtPtr",    /*tp_name*/
        sizeof(PySexpObject),   /*tp_basicsize*/
        0,                      /*tp_itemsize*/
        /* methods */
        0, /*tp_dealloc*/
        0,                      /*tp_print*/
        0,                      /*tp_getattr*/
        0,                      /*tp_setattr*/
        0,                      /*tp_compare*/
        0,                      /*tp_repr*/
        0,                      /*tp_as_number*/
        0,                      /*tp_as_sequence*/
        0,                      /*tp_as_mapping*/
        0,                      /*tp_hash*/
        0,              /*tp_call*/
        0,                      /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,     /*tp_flags*/
        ExtPtrSexp_Type_doc,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,           /*tp_methods*/
        0,                      /*tp_members*/
        ExtPtrSexp_getsets,                      /*tp_getset*/
        &Sexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)ExtPtrSexp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        /*FIXME: add new method */
        0,                     /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};
