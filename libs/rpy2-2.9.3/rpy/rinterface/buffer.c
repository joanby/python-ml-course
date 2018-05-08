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
 * Portions created by Alexander Belopolsky are 
 * Copyright (C) 2006 Alexander Belopolsky.
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
#include <Rdefines.h>

#include "_rinterface.h"
#include "buffer.h"

static int
sexp_rank(SEXP sexp)
{
  /* Return the number of dimensions for the buffer
   * (e.g., a vector will return 1, a matrix 2, ...)
   */
  SEXP dim = getAttrib(sexp, R_DimSymbol);
  if (dim == R_NilValue)
    return 1;
  return GET_LENGTH(dim);
}

static void
sexp_shape(SEXP sexp, Py_intptr_t *shape, int nd)
{
  /* Set the buffer 'shape', that is a vector of Py_intptr_t
   * containing the size of each dimension (see sexp_rank).
   */
  int i;
  SEXP dim = getAttrib(sexp, R_DimSymbol);
  if (dim == R_NilValue)
    shape[0] = LENGTH(sexp);
  else for (i = 0; i < nd; i++) {
      shape[i] = INTEGER(dim)[i];
    }
}

static void
sexp_strides(SEXP sexp, Py_intptr_t *strides, Py_ssize_t itemsize, 
	     Py_intptr_t *shape, int nd)
{
  /* Set the buffer 'strides', that is a vector or Py_intptr_t
   * containing the offset (in bytes) when progressing along
   * each dimension.
   */
  int i;
  strides[0] = itemsize;
  for (i = 1; i < nd; i++) {
    strides[i] = shape[i-1] * strides[i-1];
  }
}


#if PY_VERSION_HEX >= 0x02060000

static int
VectorSexp_getbuffer(PyObject *obj, Py_buffer *view, int flags)
{

  if (view == NULL) {
    return 0;
  }
  
  if ((flags & PyBUF_F_CONTIGUOUS) == PyBUF_F_CONTIGUOUS) {
    PyErr_SetString(PyExc_ValueError, "Only FORTRAN-style contiguous arrays allowed.");
    return -1;
  }

  view->obj = obj;
  if (obj) {
    Py_INCREF(obj);
  }
  view->readonly = 0;
  
  PySexpObject *self = (PySexpObject *)obj;
  SEXP sexp = RPY_SEXP(self);

  switch (TYPEOF(sexp)) {
  case REALSXP:
    view->buf = NUMERIC_POINTER(sexp);
    view->len = GET_LENGTH(sexp) * sizeof(double);
    view->itemsize = sizeof(double);
    view->format = "d";
    break;
  case INTSXP:
    view->buf = INTEGER_POINTER(sexp);
    view->len = GET_LENGTH(sexp) * sizeof(int);
    view->itemsize = sizeof(int);
    view->format = "i";
    break;
  case LGLSXP:
    view->buf = LOGICAL_POINTER(sexp);
    view->len = GET_LENGTH(sexp) * sizeof(int);
    view->itemsize = sizeof(int);
    view->format = "i";
    break;
  case CPLXSXP:
    view->buf = COMPLEX_POINTER(sexp);
    view->len = GET_LENGTH(sexp) * sizeof(Rcomplex);
    view->itemsize = sizeof(Rcomplex);
    view->format = "B"; /* FIXME: correct format for complex ? */
    break;
  case RAWSXP:
    view->buf = RAW_POINTER(sexp);
    view->len = GET_LENGTH(sexp);
    view->itemsize = 1;
    view->format = "B";
    break;
  default:
    PyErr_Format(PyExc_ValueError, "Buffer for this type not yet supported.");
    return -1;
  }

  view->ndim = sexp_rank(sexp);

  view->shape = NULL;
  if ((flags & PyBUF_ND) == PyBUF_ND) {
    view->shape =  (Py_intptr_t*)PyMem_Malloc(sizeof(Py_intptr_t) * view->ndim);
    sexp_shape(sexp, view->shape, view->ndim);
  }

  view->strides = NULL;
  if ((flags & PyBUF_STRIDES) == PyBUF_STRIDES) {
    view->strides = (Py_intptr_t*)PyMem_Malloc(sizeof(Py_intptr_t) * view->ndim);
    sexp_strides(sexp, view->strides, view->itemsize, view->shape, view->ndim);
  }
  /* view->suboffsets = (Py_intptr_t*)PyMem_Malloc(sizeof(Py_intptr_t) * view->ndim); */
  /* int i; */
  /* for (i = 0; i < view->ndim; i++) { */
  /*   view->suboffsets[i] = 0; */
  /* } */
  view->suboffsets = NULL;
  view->internal = NULL;
  return 0;
}
#endif


static PyBufferProcs VectorSexp_as_buffer = {

	(getbufferproc)VectorSexp_getbuffer,
	(releasebufferproc)0,
};
