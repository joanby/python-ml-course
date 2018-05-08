/*
 ***** BEGIN LICENSE BLOCK *****
 * 
 * GPLv2+ (see LICENSE file)
 *
 * Copyright (C) 2008-2017 Laurent Gautier
 *
 * ***** END LICENSE BLOCK ***** */

#include <Python.h>
#include <Rdefines.h>
#include <Rinternals.h>
#include "_rinterface.h"

#include "embeddedr.h"
#include "sexp.h"
#include "sequence.h"


/* len(x) or object.__len__() */
static Py_ssize_t VectorSexp_len(PySexpObject* object)
{
  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return -1;
  }
  embeddedR_setlock();

  Py_ssize_t len;
  /* FIXME: sanity checks. */
  SEXP sexp = RPY_SEXP(object);
  if (! sexp) {
      PyErr_Format(PyExc_ValueError, "NULL SEXP.");
      return -1;
  }
  len = (Py_ssize_t)GET_LENGTH(sexp);

  embeddedR_freelock();
  return len;
}

/* a[i] or object.__getitem__(i).
This only considers the case where 'i' is an integer.
R can also get item on names, but that's currently exposed at a higher level
in rpy2.
*/
static PyObject *
VectorSexp_item(PySexpObject* object, Py_ssize_t i)
{
  PyObject* res;
  R_len_t i_R, len_R;
  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return NULL;
  }
  embeddedR_setlock();
  SEXP *sexp = &(RPY_SEXP(object));

  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    embeddedR_freelock();
    return NULL;
  }

  len_R = GET_LENGTH(*sexp);
  
  if (i < 0) {
    /*FIXME: check that unit tests are covering this properly */
    i += len_R;
  }

  /* On 64bits platforms, Python is apparently able to use larger integer
   * than R for indexing. */
  if (i >= R_LEN_T_MAX) {
    PyErr_Format(PyExc_IndexError, "Index value exceeds what R can handle.");
    embeddedR_freelock();
    res = NULL;
    return res;
  }

  if (i < 0) {
    PyErr_Format(PyExc_IndexError, 
                 "Mysterious error: likely an integer overflow.");
    res = NULL;
    embeddedR_freelock();
    return res;
  }
  if ((i >= GET_LENGTH(*sexp))) {
    PyErr_Format(PyExc_IndexError, "Index out of range.");
    res = NULL;
  }
  else {
    double vd;
    int vi;
    Rcomplex vc;
    /* Rbyte vr; */
    char *vr;
    const char *vs;
    SEXP tmp, sexp_item, sexp_name; /* needed by LANGSXP and LISTSXP*/
    i_R = (R_len_t)i;
    switch (TYPEOF(*sexp)) {
    case REALSXP:
      vd = (NUMERIC_POINTER(*sexp))[i_R];
      if (R_IsNA(vd)) {
        res = NAReal_New(1);
      } else {
        res = PyFloat_FromDouble(vd);
      }
      break;
    case INTSXP:
      vi = INTEGER_POINTER(*sexp)[i_R];
      if (vi == NA_INTEGER) {
        res = NAInteger_New(1);
      } else {
	res = PyLong_FromLong((long)vi);
      }
      break;
    case LGLSXP:
      vi = LOGICAL_POINTER(*sexp)[i_R];
      if (vi == NA_LOGICAL) {
        res = NALogical_New(1);
      } else {
        RPY_PY_FROM_RBOOL(res, vi);
      }
      break;
    case CPLXSXP:
      vc = COMPLEX_POINTER(*sexp)[i_R];
      if (vc.r == NAREAL_IEEE.value && vc.i == NAREAL_IEEE.value) {
	res = NAComplex_New(1);
      } else {
	res = PyComplex_FromDoubles(vc.r, vc.i);
      }
      break;
    case RAWSXP:
      vr = ((char *)RAW_POINTER(*sexp)) + i_R;
      res = PyBytes_FromStringAndSize(vr, 1);
      break;
    case STRSXP:
      sexp_item = STRING_ELT(*sexp, i_R);
      if (sexp_item == NA_STRING) {
        res = NACharacter_New(1);
      } else {
	cetype_t encoding = Rf_getCharCE(sexp_item);
	switch (encoding) {
	case CE_UTF8:
	  vs = translateCharUTF8(sexp_item);
	  res = PyUnicode_FromString(vs);
	  break;
	default:
	  vs = CHAR(sexp_item);
	  res = PyUnicode_FromString(vs);
	  break;
	}
      }
      break;
/*     case CHARSXP: */
      /*       FIXME: implement handling of single char (if possible ?) */
/*       vs = (CHAR(*sexp)[i_R]); */
/*       res = PyString_FromStringAndSize(vs, 1); */
    case VECSXP:
    case EXPRSXP:
      sexp_item = VECTOR_ELT(*sexp, i_R);
      res = (PyObject *)newPySexpObject(sexp_item);
      break;
    case LISTSXP:
      /* R-exts says that it is converted to a VECSXP when subsetted */
      //tmp = nthcdr(*sexp, i_R);
      tmp = nthcdr(*sexp, i_R);
      PROTECT(sexp_item = allocVector(VECSXP,1));
      SET_VECTOR_ELT(sexp_item, 0, CAR(tmp));
      PROTECT(sexp_name = allocVector(STRSXP,1));
      SET_STRING_ELT(sexp_name, 0, PRINTNAME(TAG(tmp)));
      setAttrib(sexp_item, R_NamesSymbol, sexp_name);
      res = (PyObject *)newPySexpObject(sexp_item);
      UNPROTECT(2);
      break;      
    case LANGSXP:
      sexp_item = CAR(nthcdr(*sexp, i_R));
      res = (PyObject *)newPySexpObject(sexp_item);
      break;
    default:
      PyErr_Format(PyExc_ValueError, "Cannot handle type %d", 
                   TYPEOF(*sexp));
      res = NULL;
      break;
    }
  }
  embeddedR_freelock();
  return res;
}

/* a[i1:i2] */
static PyObject *
VectorSexp_slice(PySexpObject* object, Py_ssize_t ilow, Py_ssize_t ihigh)
{
  R_len_t len_R;

  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return NULL;
  }
  embeddedR_setlock();
  SEXP *sexp = &(RPY_SEXP(object));
  SEXP res_sexp, tmp, tmp2;

  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    embeddedR_freelock();
    return NULL;
  }

  len_R = GET_LENGTH(*sexp);

  if (ilow < 0)
    ilow = 0;
  else if (ilow > (Py_ssize_t)len_R)
    ilow = (Py_ssize_t)len_R;
  if (ihigh < ilow)
    ihigh = ilow;
  else if (ihigh > (Py_ssize_t)len_R)
    ihigh = (Py_ssize_t)len_R;

  /* On 64bits, Python is apparently able to use larger integer
   * than R for indexing. */
  if ((ilow >= (Py_ssize_t)R_LEN_T_MAX) | (ihigh >= (Py_ssize_t)R_LEN_T_MAX)) {
    PyErr_Format(PyExc_IndexError, 
                 "Index values in the slice exceed what R can handle.");
    embeddedR_freelock();
    return NULL;
  }

  if ((ilow < 0) | (ihigh < 0)) {
    PyErr_Format(PyExc_IndexError, 
                 "Mysterious error: likely an integer overflow.");
    embeddedR_freelock();
    return NULL;
  }
  if ((ilow > GET_LENGTH(*sexp)) | (ihigh > GET_LENGTH(*sexp))) {
    PyErr_Format(PyExc_IndexError, "Index out of range.");
    return NULL;
  } else {
    if ( ilow > ihigh ) {
      /* Whenever this occurs for regular Python lists,
      * a sequence of length 0 is returned. Setting ilow:=ilow
      * causes the same whithout writing "special case" code.
      */
      ihigh = ilow;
    }
    R_len_t slice_len = ihigh-ilow;
    R_len_t slice_i;
    //const char *vs;
    //SEXP tmp, sexp_item; /* tmp and sexp_item needed for case LANGSXP */
    switch (TYPEOF(*sexp)) {
    case REALSXP:
      res_sexp = allocVector(REALSXP, slice_len);
      memcpy(NUMERIC_POINTER(res_sexp),
             NUMERIC_POINTER(*sexp) + ilow,  
             (ihigh-ilow) * sizeof(double));
      break;
    case INTSXP:
      res_sexp = allocVector(INTSXP, slice_len);
      memcpy(INTEGER_POINTER(res_sexp),
             INTEGER_POINTER(*sexp) + ilow,  
             (ihigh-ilow) * sizeof(int));
      break;
    case LGLSXP:
      res_sexp = allocVector(LGLSXP, slice_len);
      memcpy(LOGICAL_POINTER(res_sexp),
             LOGICAL_POINTER(*sexp) + ilow,  
             (ihigh-ilow) * sizeof(int));
      break;
    case CPLXSXP:
      res_sexp = allocVector(CPLXSXP, slice_len);
      for (slice_i = 0; slice_i < slice_len; slice_i++) {
        COMPLEX_POINTER(res_sexp)[slice_i] = (COMPLEX_POINTER(*sexp))[slice_i + ilow];
      }
      break;
    case RAWSXP:
      res_sexp = allocVector(RAWSXP, slice_len);
      memcpy(RAW_POINTER(res_sexp),
	     RAW_POINTER(*sexp) + ilow,
	     (ihigh - ilow) * sizeof(char));
      break;
    case STRSXP:
      res_sexp = allocVector(STRSXP, slice_len);
      for (slice_i = 0; slice_i < slice_len; slice_i++) {
        SET_STRING_ELT(res_sexp, slice_i, STRING_ELT(*sexp, slice_i + ilow));
      }
      break;
/*     case CHARSXP: */
      /*       FIXME: implement handling of single char (if possible ?) */
/*       vs = (CHAR(*sexp)[i_R]); */
/*       res = PyString_FromStringAndSize(vs, 1); */
    case VECSXP:
    case EXPRSXP:
      res_sexp = allocVector(VECSXP, slice_len);
      for (slice_i = 0; slice_i < slice_len; slice_i++) {
        SET_VECTOR_ELT(res_sexp, slice_i, VECTOR_ELT(*sexp, slice_i + ilow));
      }
      break;
    case LANGSXP:
      PROTECT(res_sexp = allocList(slice_len));
      if ( slice_len > 0 ) {
	SET_TYPEOF(res_sexp, LANGSXP);
      }
      for (tmp = *sexp, tmp2 = res_sexp, slice_i = 0; 
	   slice_i < slice_len + ilow; tmp = CDR(tmp)) {
	if (slice_i - ilow > 0) {
	  tmp2 = CDR(tmp2);
	  SETCAR(tmp2, tmp);
	}
	slice_i++;
      }
      UNPROTECT(1);
      break;
    case LISTSXP:
      /* Pairlist */
      PROTECT(res_sexp = allocVector(LISTSXP, slice_len));
      tmp = res_sexp;
      for(tmp2=*sexp, slice_i=0;
	  slice_i < ihigh;
	  tmp2=CDR(tmp2), slice_i++) {
	if (slice_i >= ilow) {
	  SETCAR(tmp, CAR(tmp2));
	  SET_TAG(tmp, TAG(tmp2));
	  tmp = CDR(tmp);
	}
      }
      UNPROTECT(1);
      break;
    default:
      PyErr_Format(PyExc_ValueError, "Cannot handle R type %d", 
                   TYPEOF(*sexp));
      res_sexp = NULL;
      break;
    }

  }
  embeddedR_freelock();
  if (res_sexp == NULL) {    return NULL;
  }
  return (PyObject*)newPySexpObject(res_sexp);
}


/* a[i] = val */
static int
VectorSexp_ass_item(PySexpObject* object, Py_ssize_t i, PyObject* val)
{
  R_len_t i_R, len_R;
  int self_typeof;

  if (val == NULL) {
    PyErr_Format(PyExc_TypeError, "Object does not support item deletion.");
    return -1;
  }

  /* Check for 64 bits platforms */
  if (i >= R_LEN_T_MAX) {
    PyErr_Format(PyExc_IndexError, "Index value exceeds what R can handle.");
    return -1;
  }

  SEXP *sexp = &(RPY_SEXP(object));
  len_R = GET_LENGTH(*sexp);
  
  if (i < 0) {
    /* FIXME: Is this valid for Python < 3 ?*/
    i = len_R + i;
  }

  if (i >= len_R) {
    PyErr_Format(PyExc_IndexError, "Index out of range.");
    return -1;
  }

  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return -1;
  }
  
  int is_PySexpObject = PyObject_TypeCheck(val, &Sexp_Type);
  if (! is_PySexpObject) {
    PyErr_Format(PyExc_ValueError, "Any new value must be of "
                 "type 'Sexp_Type'.");
    return -1;
  }
  SEXP *sexp_val = &(RPY_SEXP((PySexpObject *)val));
  if (! sexp_val) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return -1;
  }

  self_typeof = TYPEOF(*sexp);

  if ( (self_typeof != VECSXP) && self_typeof != LANGSXP ) {
    if (TYPEOF(*sexp_val) != self_typeof) {
      PyErr_Format(PyExc_ValueError, 
                   "The new value cannot be of 'typeof' other than %i ('%i' given)", 
                   self_typeof, TYPEOF(*sexp_val));
      return -1;
    }
    
    if (LENGTH(*sexp_val) != 1) {
      PyErr_Format(PyExc_ValueError, "The new value must be of length 1.");
      return -1;
    }

  }

  SEXP sexp_copy;
  i_R = (R_len_t)i;
  switch (self_typeof) {
  case REALSXP:
    (NUMERIC_POINTER(*sexp))[i_R] = (NUMERIC_POINTER(*sexp_val))[0];
    break;
  case INTSXP:
    (INTEGER_POINTER(*sexp))[i_R] = (INTEGER_POINTER(*sexp_val))[0];
    break;
  case LGLSXP:
    (LOGICAL_POINTER(*sexp))[i_R] = (LOGICAL_POINTER(*sexp_val))[0];
    break;
  case CPLXSXP:
    (COMPLEX_POINTER(*sexp))[i_R] = (COMPLEX_POINTER(*sexp_val))[0];
    break;
  case RAWSXP:
    (RAW_POINTER(*sexp))[i_R] = (RAW_POINTER(*sexp_val))[0];
    break;
  case STRSXP:
    SET_STRING_ELT(*sexp, i_R, STRING_ELT(*sexp_val, 0));
    break;
  case VECSXP:
    PROTECT(sexp_copy = Rf_duplicate(*sexp_val));
    SET_VECTOR_ELT(*sexp, i_R, sexp_copy);
    UNPROTECT(1);
    break;
  case LANGSXP:
    SETCAR(nthcdr(*sexp, i_R), *sexp_val);
    break;
  default:
    PyErr_Format(PyExc_ValueError, "Cannot handle typeof '%d'", 
                 self_typeof);
    return -1;
    break;
  }
  return 0;
}

/* a[i:j] = val */
static int
VectorSexp_ass_slice(PySexpObject* object, Py_ssize_t ilow, Py_ssize_t ihigh, PyObject *val)
{
  R_len_t len_R;

  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return -1;
  }
  embeddedR_setlock();

  if (! PyObject_TypeCheck(val, &Sexp_Type)) {
    PyErr_Format(PyExc_ValueError, "Any new value must be of "
		 "type 'Sexp_Type'.");
    embeddedR_freelock();
    return -1;
  }

  SEXP *sexp = &(RPY_SEXP(object));
  len_R = GET_LENGTH(*sexp);

  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    embeddedR_freelock();
    return -1;
  }
  
  /* On 64bits, Python is apparently able to use larger integer
   * than R for indexing. */
  if ((ilow >= R_LEN_T_MAX) | (ihigh >= R_LEN_T_MAX)) {
    PyErr_Format(PyExc_IndexError, 
                 "Index values in the slice exceed what R can handle.");
    embeddedR_freelock();
    return -1;
  }

  if ((ilow < 0) | (ihigh < 0)) {
    PyErr_Format(PyExc_IndexError, 
                 "Mysterious error: likely an integer overflow.");
    embeddedR_freelock();
    return -1;
  }
  if ((ilow > GET_LENGTH(*sexp)) | (ihigh > GET_LENGTH(*sexp))) {
    PyErr_Format(PyExc_IndexError, "Index out of range.");
    return -1;
  } else {
    if ( ilow > ihigh ) {
      /* Whenever this occurs for regular Python lists,
      * a sequence of length 0 is returned. Setting ilow:=ilow
      * causes the same whithout writing "special case" code.
      */
      ihigh = ilow;
    }

    R_len_t slice_len = ihigh-ilow;
    R_len_t slice_i;

    SEXP sexp_val = RPY_SEXP((PySexpObject *)val);
    if (! sexp_val) {
      PyErr_Format(PyExc_ValueError, "NULL SEXP.");
      embeddedR_freelock();
      return -1;
    }

    if (slice_len != GET_LENGTH(sexp_val)) {
      PyErr_Format(PyExc_ValueError, "The length of the replacement value differs from the length of the slice.");
      embeddedR_freelock();
      return -1;
    }

    switch (TYPEOF(*sexp)) {
    case REALSXP:
      memcpy(NUMERIC_POINTER(*sexp) + ilow,
	     NUMERIC_POINTER(sexp_val),
             (ihigh-ilow) * sizeof(double));
      break;
    case INTSXP:
      memcpy(INTEGER_POINTER(*sexp) + ilow,
             INTEGER_POINTER(sexp_val),
             (ihigh-ilow) * sizeof(int));
      break;
    case LGLSXP:
      memcpy(LOGICAL_POINTER(*sexp) + ilow,
	     LOGICAL_POINTER(sexp_val),
             (ihigh-ilow) * sizeof(int));
      break;
    case CPLXSXP:
      for (slice_i = 0; slice_i < slice_len; slice_i++) {
        (COMPLEX_POINTER(*sexp))[slice_i + ilow] = COMPLEX_POINTER(sexp_val)[slice_i];
      }
      break;
    case RAWSXP:
      memcpy(RAW_POINTER(*sexp) + ilow,
	     RAW_POINTER(sexp_val),
	     (ihigh-ilow) * sizeof(char));
      break;
    case STRSXP:
      for (slice_i = 0; slice_i < slice_len; slice_i++) {
        SET_STRING_ELT(*sexp, slice_i + ilow, STRING_ELT(sexp_val, slice_i));
      }
      break;
    case VECSXP:
    case EXPRSXP:
      for (slice_i = 0; slice_i < slice_len; slice_i++) {
        SET_VECTOR_ELT(*sexp, slice_i + ilow, VECTOR_ELT(sexp_val, slice_i));
      }
      break;
    case CHARSXP:
    case LISTSXP:
    case LANGSXP:
    default:
      PyErr_Format(PyExc_ValueError, "Cannot handle type %d", 
                   TYPEOF(*sexp));
      embeddedR_freelock();
      return -1;
      break;
    }
  }
  embeddedR_freelock();
  return 0;
}


static PySequenceMethods VectorSexp_sequenceMethods = {
  (lenfunc)VectorSexp_len,              /* sq_length */
  0,                              /* sq_concat */
  0,                              /* sq_repeat */
  (ssizeargfunc)VectorSexp_item,        /* sq_item */
  0,                                         /* sq_slice */
  (ssizeobjargproc)VectorSexp_ass_item, /* sq_ass_item */
  0,
  0,                              /* sq_contains */
  0,                              /* sq_inplace_concat */
  0                               /* sq_inplace_repeat */
};

/* generic a[i] for Python3 */
static PyObject*
VectorSexp_subscript(PySexpObject *object, PyObject* item)
{
  Py_ssize_t i;
  if (PyIndex_Check(item)) {
    i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return NULL;
    }
    /* currently checked in VectorSexp_item */
    /* (but have it here nevertheless) */
    if (i < 0)
       i += VectorSexp_len(object);
    return VectorSexp_item(object, i);
  } 
  else if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;
    Py_ssize_t vec_len = VectorSexp_len(object);
    if (vec_len == -1)
      /* propagate the error */
      return NULL;
    if (PySlice_GetIndicesEx((PyObject*)item,
			     vec_len,
			     &start, &stop, &step, &slicelength) < 0) {
      return NULL;
    }
    if (slicelength <= 0) {
      PyErr_Format(PyExc_IndexError,
		   "The slice's length can't be < 0.");
      return NULL;
      /* return VectorSexp_New(0); */
    }
    else {
      if (step == 1) {
	PyObject *result = VectorSexp_slice(object, start, stop);
	return result;
      }
      else {
	PyErr_Format(PyExc_IndexError,
		     "Only slicing with step==1 is supported for the moment.");
	return NULL;
      }
    }
  }
  else {
    PyErr_Format(PyExc_TypeError,
		 "SexpVector indices must be integers, not %.200s",
		 Py_TYPE(item)->tp_name);
    return NULL;
  }
}

/* genericc a[i] = foo for Python 3 */
static int
VectorSexp_ass_subscript(PySexpObject* self, PyObject* item, PyObject* value)
{
    if (PyIndex_Check(item)) {
        Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
        if (i == -1 && PyErr_Occurred())
            return -1;
        if (i < 0)
            i += VectorSexp_len(self);
        return VectorSexp_ass_item(self, i, value);
    }
    else if (PySlice_Check(item)) {
      Py_ssize_t start, stop, step, slicelength;
      Py_ssize_t vec_len = VectorSexp_len(self);
      if (vec_len == -1)
      /* propagate the error */
      return -1;
      if (PySlice_GetIndicesEx((PyObject*)item, vec_len,
			       &start, &stop, &step, &slicelength) < 0) {
	return -1;
      }
      if (step == 1) {
	return VectorSexp_ass_slice(self, start, stop, value);
      } else {
	PyErr_Format(PyExc_IndexError,
		     "Only slicing with step==1 is supported for the moment.");
	return -1;
      }
    }
    else {
      PyErr_Format(PyExc_TypeError,
                     "VectorSexp indices must be integers, not %.200s",
                     item->ob_type->tp_name);
        return -1;
    }
}

static PyMappingMethods VectorSexp_as_mapping = {
  (lenfunc)VectorSexp_len,
  (binaryfunc)VectorSexp_subscript,
  (objobjargproc)VectorSexp_ass_subscript
};



static PyObject *
VectorSexp_index(PySexpObject *self, PyObject *args)
{
  Py_ssize_t i,  start, stop;
  PyObject *v;
  PyObject *item;

  SEXP sexp = RPY_SEXP(self);
  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;
  }
  start = 0;
  stop = (Py_ssize_t)(GET_LENGTH(sexp));

  if (!PyArg_ParseTuple(args, "O|O&O&:index", &v,
			_PyEval_SliceIndex, &start,
			_PyEval_SliceIndex, &stop))
    return NULL;
  if (start < 0) {
    start += (Py_ssize_t)(GET_LENGTH(sexp));
    if (start < 0)
      start = 0;
  }
  if (stop < 0) {
    stop += (Py_ssize_t)(GET_LENGTH(sexp));
    if (stop < 0)
      stop = 0;
  }
  for (i = start; i < stop && i < (Py_ssize_t)(GET_LENGTH(sexp)); i++) {
    item = VectorSexp_item(self, i);
    int cmp = PyObject_RichCompareBool(item, v, Py_EQ);
    Py_DECREF(item);
    if (cmp > 0)
      return PyLong_FromSsize_t(i);
    else if (cmp < 0)
      return NULL;
        }
  PyErr_SetString(PyExc_ValueError, "list.index(x): x not in list");
  return NULL;
  
}

PyDoc_STRVAR(VectorSexp_index_doc,
             "V.index(value, [start, [stop]]) -> integer -- return first index of value."
             "Raises ValueError if the value is not present.");

static PyMethodDef VectorSexp_methods[] = {
  {"index", (PyCFunction)VectorSexp_index, METH_VARARGS, VectorSexp_index_doc},
  {NULL, NULL}
};
  

static PyGetSetDef VectorSexp_getsets[] = {
  {"__array_struct__",
   (getter)array_struct_get,
   (setter)0,
   "Array protocol: struct"},
  {NULL, NULL, NULL, NULL}          /* sentinel */
};


PyDoc_STRVAR(VectorSexp_Type_doc,
             "R object that is a vector."
             " R vectors start their indexing at one,"
             " while Python lists or arrays start indexing"
             " at zero.\n"
             "In the hope to avoid confusion, the indexing"
             " in Python (e.g., :meth:`__getitem__` / :meth:`__setitem__`)"
             " starts at zero.");

static int
VectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds);

static PyTypeObject VectorSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.SexpVector",        /*tp_name*/
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
        &VectorSexp_sequenceMethods,                    /*tp_as_sequence*/
	&VectorSexp_as_mapping,
        0,                      /*tp_hash*/
        0,              /*tp_call*/
        0,              /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
        &VectorSexp_as_buffer,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,  /*tp_flags*/
        VectorSexp_Type_doc,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        VectorSexp_methods,           /*tp_methods*/
        0,                      /*tp_members*/
        VectorSexp_getsets,            /*tp_getset*/
        &Sexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)VectorSexp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        0,               /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};

static int
VectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds)
{

#ifdef RPY_VERBOSE
  printf("%p: VectorSexp initializing...\n", self);
#endif 

  if (! (rpy_has_status(RPY_R_INITIALIZED))) {
    PyErr_Format(PyExc_RuntimeError, 
                 "R must be initialized before any instance can be created.");
    return -1;
  }

  PyObject *object;
  int sexptype = -1;
  static char *kwlist[] = {"sexpvector", "sexptype", NULL};


  /* FIXME: handle the copy argument */
  if (! PyArg_ParseTupleAndKeywords(args, kwds, "O|i", 
                                    kwlist,
                                    &object,
                                    &sexptype)) {
    return -1;
  }

  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return -1;
  }
  embeddedR_setlock();

  if (PyObject_IsInstance(object, 
                          (PyObject*)&VectorSexp_Type)) {
    /* call parent's constructor */
      if (Sexp_init(self, args, NULL) == -1) {
	/* PyErr_Format(PyExc_RuntimeError, "Error initializing instance."); */
	embeddedR_freelock();
	return -1;
      }
  } else if (PySequence_Check(object)) {
    if ((sexptype < 0) || (sexptype > RPY_MAX_VALIDSEXTYPE) || 
        (! validSexpType[sexptype])) {
      PyErr_Format(PyExc_ValueError, "Invalid SEXP type '%i'.", sexptype);
      embeddedR_freelock();
      return -1;
    }
    /* FIXME: implemement automagic type ?
     *(RPy has something)... or leave it to extensions ? 
     */
    SEXP sexp = newSEXP(object, sexptype);
    PROTECT(sexp); /* sexp is not preserved*/
    if (sexp == NULL) {
      /* newSEXP returning NULL will also have raised an exception
       * (not-so-clear design :/ )
       */
      UNPROTECT(1);
      embeddedR_freelock();
      return -1;
    }
    if (Rpy_ReplaceSexp((PySexpObject *)self, sexp) == -1) {
      embeddedR_freelock();
      UNPROTECT(1);
      return -1;
    }
    UNPROTECT(1);

    #ifdef RPY_DEBUG_OBJECTINIT
    printf("  SEXP vector is %p.\n", RPY_SEXP((PySexpObject *)self));
    #endif
    /* SET_NAMED(RPY_SEXP((PySexpObject *)self), 2); */
  } else {
    PyErr_Format(PyExc_ValueError, "Invalid sexpvector.");
    embeddedR_freelock();
    return -1;
  }

#ifdef RPY_VERBOSE
  printf("done (VectorSexp_init).\n");
#endif 

  embeddedR_freelock();
  return 0;
}


/* transition to replace the current VectorSexp_init()
   and make VectorSexp_init() an abstract class */
static int
VectorSexp_init_private(PyObject *self, PyObject *args, PyObject *kwds,
			RPy_seqobjtosexpproc seq_to_R, 
			RPy_iterobjtosexpproc iter_to_R, int sexptype)
{

  if (! (rpy_has_status(RPY_R_INITIALIZED))) {
    PyErr_Format(PyExc_RuntimeError, 
                 "R must be initialized before any instance can be created.");
    return -1;
  }

  PyObject *object;
  PySexpObject *rpyobject;
  static char *kwlist[] = {"sexpvector", NULL};

  if (! PyArg_ParseTupleAndKeywords(args, kwds, "O", 
                                    kwlist,
                                    &object)) {
    return -1;
  }

  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return -1;
  }
  embeddedR_setlock();
  SEXP sexp = R_NilValue;
  if (PyObject_IsInstance(object, 
                          (PyObject*)&VectorSexp_Type)) {
#ifdef RPY_VERBOSE
    printf("    object already a VectorSexp_Type\n");
#endif 

    rpyobject = (PySexpObject *)object;
    if (sexptype != TYPEOF(RPY_SEXP(rpyobject))) {
      PyErr_Format(PyExc_ValueError, "Invalid SEXP type '%i' (should be %i).", 
		   TYPEOF(RPY_SEXP(rpyobject)), sexptype);
      embeddedR_freelock();
      return -1;
    }
    /* call parent's constructor */
    if (Sexp_init(self, args, NULL) == -1) {
      /* PyErr_Format(PyExc_RuntimeError, "Error initializing instance."); */
      embeddedR_freelock();
      return -1;
    }
  } else {
    /* The parameter is not already a PySexpObject. Create
     the necessary PySexpObjects. */
    int is_sequence = PySequence_Check(object);
    if ( !is_sequence ) {
      Py_ssize_t length = PyObject_Length(object);
      if (length == -1) {
	PyErr_Format(PyExc_ValueError,
		     "The object does not have a length.");
	embeddedR_freelock();	
	return -1;
      }
      if (PyIter_Check(object)) {
	if (iter_to_R == NULL) {
	  /*FIXME: temporary, while the different implementations are written */
	} else if (iter_to_R(object, length, &sexp) == -1) {
	  /* RPy_SeqTo*SXP returns already raises an exception in case of problem
	   */
	  embeddedR_freelock();
	  return -1;
	}
      }
      PyErr_Format(PyExc_ValueError,
		   "Unexpected problem when building R vector from non-sequence.");
      embeddedR_freelock();	
      return -1;
    } else {
#ifdef RPY_VERBOSE
      printf("    object a sequence\n");
#endif 
      
      if (seq_to_R(object, &sexp) == -1) {
	/* RPy_SeqTo*SXP returns already raises an exception in case of problem
	 */
	embeddedR_freelock();
	return -1;
      }
      
      //R_PreserveObject(sexp);
#ifdef RPY_DEBUG_PRESERVE
      preserved_robjects += 1;
      printf("  PRESERVE -- R_PreserveObject -- %p -- %i\n", 
	     sexp, preserved_robjects);
#endif  
      if (Rpy_ReplaceSexp((PySexpObject *)self, sexp) == -1) {
	embeddedR_freelock();
	return -1;
      }
#ifdef RPY_DEBUG_OBJECTINIT
      printf("  SEXP vector is %p.\n", RPY_SEXP((PySexpObject *)self));
#endif
      /* SET_NAMED(RPY_SEXP((PySexpObject *)self), 2); */
    }
  } 

  embeddedR_freelock();
  return 0;
}


 
PyDoc_STRVAR(IntVectorSexp_Type_doc,
             "R vector of integers (note: integers in R are C-int, not C-long)");

static int
IntVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds);

static PyTypeObject IntVectorSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.IntSexpVector",        /*tp_name*/
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
        0,                    /*tp_as_sequence*/
	0,
        0,                      /*tp_hash*/
        0,              /*tp_call*/
        0,              /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
	0,                      /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,  /*tp_flags*/
        IntVectorSexp_Type_doc,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,           /*tp_methods*/
        0,                      /*tp_members*/
        0,            /*tp_getset*/
        &VectorSexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)IntVectorSexp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        0,               /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};


/* Take an arbitray Python sequence and a target pointer SEXP
   and build an R vector of integers.
   The function returns 0 on success, -1 on failure. In the case
   of a failure, it will also create an exception with an informative
   message that can be propagated up.
*/
static int
RPy_SeqToINTSXP(PyObject *object, SEXP *sexpp)
{
  Py_ssize_t ii;
  PyObject *seq_object, *item, *item_tmp;
  SEXP new_sexp;
 
  seq_object = PySequence_Fast(object,
			       "Cannot create R object from non-sequence object.");
  if (! seq_object) {
    return -1;
  }

  const Py_ssize_t length = PySequence_Fast_GET_SIZE(seq_object);

  if (length > R_LEN_T_MAX) {
    PyErr_Format(PyExc_ValueError,
		 "The Python sequence is longer than the longuest possible vector in R");
    Py_XDECREF(seq_object);
    return -1;
  }

  PROTECT(new_sexp = NEW_INTEGER(length));
  int *integer_ptr = INTEGER(new_sexp);

  /*FIXME: Optimization possible for array.array by using memcpy().
   * With Python >= 2.7, this could be extended to memoryviews.
   */
  for (ii = 0; ii < length; ++ii) {
    item = PySequence_Fast_GET_ITEM(seq_object, ii);
    item_tmp = PyNumber_Long(item);
    if (item == NAInteger_New(0)) {
      integer_ptr[ii] = NA_INTEGER;
    } else if (item_tmp) {
      long l = PyLong_AS_LONG(item_tmp);
      if ((l > (long)INT_MAX) || (l < (long)INT_MIN)) {
	UNPROTECT(1);
	PyErr_Format(PyExc_OverflowError,
		     "Integer overflow with element %zd.",
		     ii);
	Py_XDECREF(seq_object);
	return -1;
      } else {
	integer_ptr[ii] = (int)l;
      }
    } else {
      UNPROTECT(1);
      PyErr_Format(PyExc_ValueError,
		   "Error while trying to convert element %zd to an integer.",
		   ii);
      Py_XDECREF(seq_object);
      return -1;
    }
    Py_XDECREF(item_tmp);
  }
  UNPROTECT(1);
  *sexpp = new_sexp;
  Py_XDECREF(seq_object);
  return 0;
}

/* Take an arbitray Python iterable, a length, and a target pointer SEXP
   and build an R vector of integers.
   The function returns 0 on success, -1 on failure. In the case
   of a failure, it will also create an exception with an informative
   message that can be propagated up.
*/
static int
RPy_IterToINTSXP(PyObject *object, const Py_ssize_t length, SEXP *sexpp)
{

  PyObject *item, *item_tmp;
  SEXP new_sexp;


  if (length > R_LEN_T_MAX) {
    PyErr_Format(PyExc_ValueError,
		 "The length exceeds what the longuest possible R vector can be.");
  }

  PROTECT(new_sexp = NEW_INTEGER(length));
  int *integer_ptr = INTEGER(new_sexp);

  Py_ssize_t ii = 0;
  while (ii < length) {

    item = PyIter_Next(object);

    if (item == NULL) {
      UNPROTECT(1);
      PyErr_Format(PyExc_ValueError,
		   "Error while trying to retrive element %zd in the iterator.",
		   ii);
      return -1;
    }

    
    item_tmp = PyNumber_Long(item);
    if (item == NAInteger_New(0)) {
      integer_ptr[ii] = NA_INTEGER;
    } else if (item_tmp) {
      long l = PyLong_AS_LONG(item_tmp);
      if ((l > (long)INT_MAX) || (l < (long)INT_MIN)) {
	UNPROTECT(1);
	PyErr_Format(PyExc_OverflowError,
		     "Integer overflow with element %zd.",
		     ii);	
	return -1;
      } else {
	integer_ptr[ii] = (int)l;
      }
    } else {
      UNPROTECT(1);
      PyErr_Format(PyExc_ValueError,
		   "Error while trying to convert element %zd to an integer.",
		   ii);
      return -1;
    }
    Py_XDECREF(item_tmp);
    ii++;
  }
  UNPROTECT(1);
  *sexpp = new_sexp;
  return 0;
}


/* Make an R INTSEXP from a Python int or long scalar */
static SEXP 
IntVectorSexp_AsSexp(PyObject *pyfloat) {
  int status;
  SEXP sexp;
  PyObject *seq_tmp = PyTuple_New(1);
  PyTuple_SetItem(seq_tmp, 0, pyfloat);
  status = RPy_SeqToINTSXP(seq_tmp, &sexp);
  if (status == -1) {
    return NULL;
  }
  Py_DECREF(seq_tmp);
  return sexp;
}

static int
IntVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds)
{
#ifdef RPY_VERBOSE
  printf("%p: IntVectorSexp initializing...\n", self);
#endif 
  int res;

  res = VectorSexp_init_private(self, args, kwds, 
				(RPy_seqobjtosexpproc)RPy_SeqToINTSXP,
				(RPy_iterobjtosexpproc)RPy_IterToINTSXP,
				INTSXP);
#ifdef RPY_VERBOSE
  printf("done (IntVectorSexp_init).\n");
#endif 
  return res;
}



PyDoc_STRVAR(FloatVectorSexp_Type_doc,
             "R vector of Python floats (note: double in C)");

static int
FloatVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds);

static PyTypeObject FloatVectorSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.FloatSexpVector",        /*tp_name*/
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
        0,                    /*tp_as_sequence*/
        0,                      /*tp_as_mapping*/
        0,                      /*tp_hash*/
        0,              /*tp_call*/
        0,              /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
	0,                      /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,  /*tp_flags*/
        FloatVectorSexp_Type_doc,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,           /*tp_methods*/
        0,                      /*tp_members*/
        0,            /*tp_getset*/
        &VectorSexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)FloatVectorSexp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        0,               /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};


/* Take an arbitray Python sequence and a target pointer SEXP
   and build an R vector of "numeric" values (double* in C, float in Python).
   The function returns 0 on success, -1 on failure. In the case
   of a failure, it will also create an exception with an informative
   message that can be propagated up.
*/
static int
RPy_SeqToREALSXP(PyObject *object, SEXP *sexpp)
{
  Py_ssize_t ii;
  PyObject *seq_object, *item, *item_tmp;
  SEXP new_sexp;
 
  seq_object = PySequence_Fast(object,
			       "Cannot create R object from non-sequence object.");
  if (! seq_object) {
    return -1;
  }

  const Py_ssize_t length = PySequence_Fast_GET_SIZE(seq_object);

  if (length > R_LEN_T_MAX) {
    PyErr_Format(PyExc_ValueError,
		 "The Python sequence is longer than the longuest possible vector in R");
    Py_XDECREF(seq_object);
    return -1;
  }

  PROTECT(new_sexp = NEW_NUMERIC(length));
  double *double_ptr = NUMERIC_POINTER(new_sexp);
  /*FIXME: Optimization possible for array.array by using memcpy().
   * With Python >= 2.7, this could be extended to memoryviews.
   */
  for (ii = 0; ii < length; ++ii) {
    item = PySequence_Fast_GET_ITEM(seq_object, ii);
    item_tmp = PyNumber_Float(item);
    if (item == NAReal_New(0)) {
      double_ptr[ii] = NA_REAL;
    } else if (item_tmp) {
      double value = PyFloat_AS_DOUBLE(item_tmp);
      double_ptr[ii] = value;
    } else {
      UNPROTECT(1);
      PyErr_Format(PyExc_ValueError,
		   "Error while trying to convert element %zd to a double.",
		   ii);
      Py_XDECREF(seq_object);
      return -1;
    }
    Py_XDECREF(item_tmp);
  }
  UNPROTECT(1);
  *sexpp = new_sexp;
  Py_XDECREF(seq_object);
  return 0;
}
/* Take an arbitray Python iterator, length, and a target pointer SEXP
   and build an R vector of "numeric" values (double* in C, float in Python).
   The function returns 0 on success, -1 on failure. In the case
   of a failure, it will also create an exception with an informative
   message that can be propagated up.
*/
static int
RPy_IterToREALSXP(PyObject *object, Py_ssize_t length, SEXP *sexpp)
{
  PyObject *item, *item_tmp;
  SEXP new_sexp;
 
  if (length > R_LEN_T_MAX) {
    PyErr_Format(PyExc_ValueError,
		 "The Python sequence is longer than the longuest possible vector in R");
  }
  
  PROTECT(new_sexp = NEW_NUMERIC(length));
  double *double_ptr = NUMERIC_POINTER(new_sexp);
  
  Py_ssize_t ii = 0;
  while (ii < length) {
    item = PyIter_Next(object);
    if (item == NULL) {
      UNPROTECT(1);
      PyErr_Format(PyExc_ValueError,
		   "Error while trying to retrive element %zd in the iterator.",
		   ii);
      return -1;
    }
    item_tmp = PyNumber_Float(item);
    if (item == NAReal_New(0)) {
      double_ptr[ii] = NA_REAL;
    } else if (item_tmp) {
      double value = PyFloat_AS_DOUBLE(item_tmp);
      double_ptr[ii] = value;
    } else {
      UNPROTECT(1);
      PyErr_Format(PyExc_ValueError,
		   "Error while trying to convert element %zd to a double.",
		   ii);
      return -1;
    }
    Py_XDECREF(item_tmp);
    ii++;
  }
  UNPROTECT(1);
  *sexpp = new_sexp;
  return 0;
}

/* Make an R NUMERIC SEXP from a Python float scalar */
static SEXP 
FloatVectorSexp_AsSexp(PyObject *pyfloat) {
  int status;
  SEXP sexp;
  PyObject *seq_tmp = PyTuple_New(1);
  PyTuple_SetItem(seq_tmp, 0, pyfloat);
  status = RPy_SeqToREALSXP(seq_tmp, &sexp);
  if (status == -1) {
    return NULL;
  }
  Py_DECREF(seq_tmp);
  return sexp;
}

static int
FloatVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds)
{
#ifdef RPY_VERBOSE
  printf("%p: FloatVectorSexp initializing...\n", self);
#endif 
  int res = VectorSexp_init_private(self, args, kwds, 
				    (RPy_seqobjtosexpproc)RPy_SeqToREALSXP, 
				    (RPy_iterobjtosexpproc)RPy_IterToREALSXP, 
				    REALSXP);
#ifdef RPY_VERBOSE
  printf("done (FloatVectorSexp_init).\n");
#endif 
  return res;
}




PyDoc_STRVAR(StrVectorSexp_Type_doc,
             "R vector of Python strings");

static int
StrVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds);

static PyTypeObject StrVectorSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.StrSexpVector",        /*tp_name*/
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
        0,                    /*tp_as_sequence*/
        0,                      /*tp_as_mapping*/
        0,                      /*tp_hash*/
        0,              /*tp_call*/
        0,              /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
	0,                       /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,  /*tp_flags*/
        StrVectorSexp_Type_doc,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,           /*tp_methods*/
        0,                      /*tp_members*/
        0,            /*tp_getset*/
        &VectorSexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)StrVectorSexp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        0,               /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};


/* Take an arbitray Python sequence and a target pointer SEXP
   and build an R vector of strings (character in R, char* in C).
   The function returns 0 on success, -1 on failure. In the case
   of a failure, it will also create an exception with an informative
   message that can be propagated up.
*/
static int
RPy_SeqToSTRSXP(PyObject *object, SEXP *sexpp)
{
  Py_ssize_t ii;
  PyObject *seq_object, *item, *item_tmp;
  SEXP new_sexp, str_R;
 
  seq_object = PySequence_Fast(object,
			       "Cannot create R object from non-sequence object.");
  if (! seq_object) {
    return -1;
  }

  const Py_ssize_t length = PySequence_Fast_GET_SIZE(seq_object);

  if (length > R_LEN_T_MAX) {
    PyErr_Format(PyExc_ValueError,
		 "The Python sequence is longer than the longuest possible vector in R");
    Py_XDECREF(seq_object);
    return -1;
  }

  PROTECT(new_sexp = NEW_CHARACTER(length));

  for (ii = 0; ii < length; ++ii) {
    item = PySequence_Fast_GET_ITEM(seq_object, ii);

    if (item == NACharacter_New(0)) {
      SET_STRING_ELT(new_sexp, ii, NA_STRING);
      continue;
    }
    
    /* Only difference with Python < 3.1 is that PyString case is dropped. 
       Technically a macro would avoid code duplication.
    */
    if (PyUnicode_Check(item)) {
      item_tmp = PyUnicode_AsUTF8String(item);
      if (item_tmp == NULL) {
	UNPROTECT(1);
	PyErr_Format(PyExc_ValueError,
		     "Error raised by codec for element %zd.",
		     ii);
	Py_XDECREF(seq_object);
	return -1;	
      }
      const char *string = PyBytes_AsString(item_tmp);
      str_R = Rf_mkCharCE(string, CE_UTF8);
    }
    else {
      /* Last option: try to call str() on the object. */
      item_tmp = PyObject_Str(item);
      if (item_tmp == NULL) {
	UNPROTECT(1);
	PyErr_Format(PyExc_ValueError,
		     "Error raised when calling str() for element %zd.",
		     ii);
	Py_XDECREF(seq_object);
	return -1;	
      }
      PyObject *item_tmp2 = PyUnicode_AsUTF8String(item_tmp);
      if (item_tmp2 == NULL) {
	UNPROTECT(1);
	PyErr_Format(PyExc_ValueError,
		     "Error raised by codec for str(element %zd).",
		     ii);
	Py_XDECREF(seq_object);
	return -1;	
      }
      const char *string = PyBytes_AsString(item_tmp2);
      str_R = Rf_mkCharCE(string, CE_UTF8);
      Py_DECREF(item_tmp2);
    }
    
    SET_STRING_ELT(new_sexp, ii, str_R);
    Py_XDECREF(item_tmp);
  }
  UNPROTECT(1);
  *sexpp = new_sexp;
  Py_XDECREF(seq_object);
  return 0;
}

/* Make an R STRSEXP from a Python string scalar */
static SEXP 
StrVectorSexp_AsSexp(PyObject *pyfloat) {
  int status;
  SEXP sexp;
  PyObject *seq_tmp = PyTuple_New(1);
  PyTuple_SetItem(seq_tmp, 0, pyfloat);
  status = RPy_SeqToSTRSXP(seq_tmp, &sexp);
  if (status == -1) {
    return NULL;
  }
  Py_DECREF(seq_tmp);
  return sexp;
}

static int
StrVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds)
{
#ifdef RPY_VERBOSE
  printf("%p: StrVectorSexp initializing...\n", self);
#endif 
  int res = VectorSexp_init_private(self, args, kwds, 
				    (RPy_seqobjtosexpproc)RPy_SeqToSTRSXP,
				    NULL,
				    STRSXP);
#ifdef RPY_VERBOSE
  printf("done (StrVectorSexp_init).\n");
#endif 
  return res;
}



PyDoc_STRVAR(BoolVectorSexp_Type_doc,
             "R vector of booleans");

static int
BoolVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds);

static PyTypeObject BoolVectorSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.BoolSexpVector",        /*tp_name*/
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
        0,                    /*tp_as_sequence*/
        0,                      /*tp_as_mapping*/
        0,                      /*tp_hash*/
        0,              /*tp_call*/
        0,              /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
	0,                      /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,  /*tp_flags*/
        BoolVectorSexp_Type_doc,/*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,           /*tp_methods*/
        0,                      /*tp_members*/
        0,            /*tp_getset*/
        &VectorSexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)BoolVectorSexp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        0,               /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};


/* Take an arbitray Python sequence and a target pointer SEXP
   and build an R vector of "logical" values (booleans).
   The function returns 0 on success, -1 on failure. In the case
   of a failure, it will also create an exception with an informative
   message that can be propagated up.
*/
static int
RPy_SeqToLGLSXP(PyObject *object, SEXP *sexpp)
{
  Py_ssize_t ii;
  PyObject *seq_object, *item;
  SEXP new_sexp;
 
  seq_object = PySequence_Fast(object,
			       "Cannot create R object from non-sequence object.");
  if (! seq_object) {
    return -1;
  }

  const Py_ssize_t length = PySequence_Fast_GET_SIZE(seq_object);

  if (length > R_LEN_T_MAX) {
    PyErr_Format(PyExc_ValueError,
		 "The Python sequence is longer than the longuest possible vector in R");
    Py_XDECREF(seq_object);
    return -1;
  }

  PROTECT(new_sexp = NEW_LOGICAL(length));
  int *int_ptr = LOGICAL_POINTER(new_sexp);
  /*FIXME: Optimization possible for array.array by using memcpy().
   * With Python >= 2.7, this could be extended to memoryviews.
   */
  for (ii = 0; ii < length; ++ii) {
    item = PySequence_Fast_GET_ITEM(seq_object, ii);

    if (item == NALogical_New(0)) {
    /* Special case: NA value from R */
      int_ptr[ii] = NA_LOGICAL;
    } else {
      int isnot = PyObject_Not(item);
      switch(isnot) {
      case 0:
	int_ptr[ii] = TRUE;
	break;
      case 1:
	int_ptr[ii] = FALSE;
	break;
      case -1:
	UNPROTECT(1);
	/* FIXME: PyObject_Not() will have raised an exception,
	* may be the text for the exception should be reported ?*/
	PyErr_Format(PyExc_ValueError,
		     "Error while evaluating 'not <element %zd>'.",
		   ii);
	Py_XDECREF(seq_object);
	return -1;
	break;
      }
    }
  }
  UNPROTECT(1);
  *sexpp = new_sexp;
  Py_XDECREF(seq_object);
  return 0;
}

/* Make an R LGLSEXP from a Python bool scalar */
static SEXP 
BoolVectorSexp_AsSexp(PyObject *pyfloat) {
  int status;
  SEXP sexp;
  PyObject *seq_tmp = PyTuple_New(1);
  PyTuple_SetItem(seq_tmp, 0, pyfloat);
  status = RPy_SeqToLGLSXP(seq_tmp, &sexp);
  if (status == -1) {
    return NULL;
  }
  Py_DECREF(seq_tmp);
  return sexp;
}


static int
BoolVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds)
{
#ifdef RPY_VERBOSE
  printf("%p: BoolVectorSexp initializing...\n", self);
#endif 
  int res = VectorSexp_init_private(self, args, kwds, 
				    (RPy_seqobjtosexpproc)RPy_SeqToLGLSXP, 
				    NULL,
				    LGLSXP);
#ifdef RPY_VERBOSE
  printf("done (BoolVectorSexp_init).\n");
#endif 
  return res;
}


PyDoc_STRVAR(ByteVectorSexp_Type_doc,
             "R vector of bytes");

static int
ByteVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds);

static PyTypeObject ByteVectorSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.ByteSexpVector",        /*tp_name*/
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
        0,                    /*tp_as_sequence*/
        0,                      /*tp_as_mapping*/
        0,                      /*tp_hash*/
        0,              /*tp_call*/
        0,              /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
	0,                      /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,  /*tp_flags*/
        ByteVectorSexp_Type_doc,/*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,           /*tp_methods*/
        0,                      /*tp_members*/
        0,            /*tp_getset*/
        &VectorSexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)ByteVectorSexp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        0,               /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};


/* Take an arbitray Python sequence and a target pointer SEXP
   and build an R vector of "raw" values (bytes).
   The function returns 0 on success, -1 on failure. In the case
   of a failure, it will also create an exception with an informative
   message that can be propagated up.
*/
static int
RPy_SeqToRAWSXP(PyObject *object, SEXP *sexpp)
{
  Py_ssize_t ii;
  PyObject *seq_object, *item;
  SEXP new_sexp;
 
  seq_object = PySequence_Fast(object,
			       "Cannot create R object from non-sequence object.");
  if (! seq_object) {
    return -1;
  }

  const Py_ssize_t length = PySequence_Fast_GET_SIZE(seq_object);

  if (length > R_LEN_T_MAX) {
    PyErr_Format(PyExc_ValueError,
		 "The Python sequence is longer than the longuest possible vector in R");
    Py_XDECREF(seq_object);
    return -1;
  }

  PROTECT(new_sexp = NEW_RAW(length));
  char *raw_ptr = (char *)RAW_POINTER(new_sexp);
  /*FIXME: Optimization possible for array.array by using memcpy().
   * With Python >= 2.7, this could be extended to memoryviews.
   */
  for (ii = 0; ii < length; ++ii) {
    item = PySequence_Fast_GET_ITEM(seq_object, ii);
    Py_ssize_t size_tmp;
    char *buffer;
    int ok;
    ok = PyBytes_AsStringAndSize(item, &buffer, &size_tmp);
    if (ok == -1) {
      UNPROTECT(1);
      PyErr_Format(PyExc_ValueError,
		   "Element %zd is not a byte.",
		   ii);
      Py_XDECREF(seq_object);
      return -1;      
    } else if (size_tmp > 1) {
      UNPROTECT(1);
      PyErr_Format(PyExc_ValueError,
		   "Element %zd contains more than one byte.",
		   ii);
      Py_XDECREF(seq_object);
      return -1;
    }
    raw_ptr[ii] = buffer[0];
  }
  UNPROTECT(1);
  *sexpp = new_sexp;
  Py_XDECREF(seq_object);
  return 0;
}

static int
ByteVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds)
{
#ifdef RPY_VERBOSE
  printf("%p: ByteVectorSexp initializing...\n", self);
#endif 
  int res = VectorSexp_init_private(self, args, kwds, 
				    (RPy_seqobjtosexpproc)RPy_SeqToRAWSXP,
				    NULL,
				    RAWSXP);
#ifdef RPY_VERBOSE
  printf("done (ByteVectorSexp_init).\n");
#endif 
  return res;
}

PyDoc_STRVAR(ComplexVectorSexp_Type_doc,
             "R vector of complex values.");

static int
ComplexVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds);

static PyTypeObject ComplexVectorSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.ComplexSexpVector",        /*tp_name*/
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
        0,                    /*tp_as_sequence*/
        0,                      /*tp_as_mapping*/
        0,                      /*tp_hash*/
        0,              /*tp_call*/
        0,              /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
	0,                      /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,  /*tp_flags*/
        ComplexVectorSexp_Type_doc,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,           /*tp_methods*/
        0,                      /*tp_members*/
        0,            /*tp_getset*/
        &VectorSexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)ComplexVectorSexp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        0,               /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};


/* Take an arbitray Python sequence and a target pointer SEXP
   and build an R vector of "complex" values.
   The function returns 0 on success, -1 on failure. In the case
   of a failure, it will also create an exception with an informative
   message that can be propagated up.
*/
static int
RPy_SeqToCPLXSXP(PyObject *object, SEXP *sexpp)
{
  Py_ssize_t ii;
  PyObject *seq_object, *item;
  SEXP new_sexp;
 
  seq_object = PySequence_Fast(object,
			       "Cannot create R object from non-sequence object.");
  if (! seq_object) {
    return -1;
  }

  const Py_ssize_t length = PySequence_Fast_GET_SIZE(seq_object);

  if (length > R_LEN_T_MAX) {
    PyErr_Format(PyExc_ValueError,
		 "The Python sequence is longer than the longuest possible vector in R");
    Py_XDECREF(seq_object);
    return -1;
  }

  PROTECT(new_sexp = NEW_COMPLEX(length));
  /*FIXME: Optimization possible for array.array by using memcpy().
   * With Python >= 2.7, this could be extended to memoryviews.
   */
  for (ii = 0; ii < length; ++ii) {
    item = PySequence_Fast_GET_ITEM(seq_object, ii);
    if (item == NAComplex_New(0)) {
      COMPLEX(new_sexp)[ii].r = NA_REAL;
      COMPLEX(new_sexp)[ii].i = NA_REAL;
    } else if (PyComplex_Check(item)) {
      Py_complex cplx = PyComplex_AsCComplex(item);
      COMPLEX(new_sexp)[ii].r = cplx.real;
      COMPLEX(new_sexp)[ii].i = cplx.imag;
    } else {
      UNPROTECT(1);
      PyErr_Format(PyExc_ValueError,
		   "Element %zd is not a complex",
		   ii);
      Py_XDECREF(seq_object);
      return -1;
    }
  }
  UNPROTECT(1);
  *sexpp = new_sexp;
  Py_XDECREF(seq_object);
  return 0;
}

/* Make an R LGLSEXP from a Python complex scalar */
static SEXP 
ComplexVectorSexp_AsSexp(PyObject *pyfloat) {
  int status;
  SEXP sexp;
  PyObject *seq_tmp = PyTuple_New(1);
  PyTuple_SetItem(seq_tmp, 0, pyfloat);
  status = RPy_SeqToCPLXSXP(seq_tmp, &sexp);
  if (status == -1) {
    return NULL;
  }
  Py_DECREF(seq_tmp);
  return sexp;
}

static int
ComplexVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds)
{
#ifdef RPY_VERBOSE
  printf("%p: ComplexVectorSexp initializing...\n", self);
#endif 
  int res = VectorSexp_init_private(self, args, kwds, 
				    (RPy_seqobjtosexpproc)RPy_SeqToCPLXSXP, 
				    NULL,
				    CPLXSXP);
#ifdef RPY_VERBOSE
  printf("done (ComplexVectorSexp_init).\n");
#endif 
  return res;
}


PyDoc_STRVAR(ListVectorSexp_Type_doc,
             "R list.");

static int
ListVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds);

static PyTypeObject ListVectorSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.ListSexpVector",        /*tp_name*/
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
        0,                    /*tp_as_sequence*/
        0,                      /*tp_as_mapping*/
        0,                      /*tp_hash*/
        0,              /*tp_call*/
        0,              /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
	0,                      /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,  /*tp_flags*/
        ListVectorSexp_Type_doc,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        0,           /*tp_methods*/
        0,                      /*tp_members*/
        0,            /*tp_getset*/
        &VectorSexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)ListVectorSexp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        0,               /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};


/* Take an arbitray Python sequence and a target pointer SEXP
   and build an R list.
   The function returns 0 on success, -1 on failure. In the case
   of a failure, it will also create an exception with an informative
   message that can be propagated up.
*/
static int
RPy_SeqToVECSXP(PyObject *object, SEXP *sexpp)
{
  Py_ssize_t ii;
  PyObject *seq_object, *item;
  SEXP new_sexp, new_sexp_item;
 
  seq_object = PySequence_Fast(object,
			       "Cannot create R object from non-sequence object.");
  if (! seq_object) {
    return -1;
  }

  const Py_ssize_t length = PySequence_Fast_GET_SIZE(seq_object);

  if (length > R_LEN_T_MAX) {
    PyErr_Format(PyExc_ValueError,
		 "The Python sequence is longer than the longuest possible vector in R");
    Py_XDECREF(seq_object);
    return -1;
  }

  PROTECT(new_sexp = NEW_LIST(length));

  for (ii = 0; ii < length; ++ii) {
    item = PySequence_Fast_GET_ITEM(seq_object, ii);
    
    if (PyObject_TypeCheck(item, &Sexp_Type)) {
      /* if element in the list already represents an R object, 
       * add it as is */
      SET_ELEMENT(new_sexp, ii, RPY_SEXP((PySexpObject *)item));
    } else if (PyFloat_Check(item)) {
      /* if element is a float, put it silently into a vector of length 1 */
      /* FIXME: PROTECT ? */
      new_sexp_item = FloatVectorSexp_AsSexp(item);
      if (new_sexp_item) {
	SET_ELEMENT(new_sexp, ii, new_sexp_item);
      } else {
	UNPROTECT(1);
	Py_XDECREF(seq_object);
	return -1;
      }
    } else if (PyBool_Check(item)) {
      new_sexp_item = BoolVectorSexp_AsSexp(item);
      if (new_sexp_item) {
	SET_ELEMENT(new_sexp, ii, new_sexp_item);
      } else {
	UNPROTECT(1);
	Py_XDECREF(seq_object);
	return -1;
      }
    } else if (PyLong_Check(item)
        ) {
      new_sexp_item = IntVectorSexp_AsSexp(item);
      if (new_sexp_item) {
	SET_ELEMENT(new_sexp, ii, new_sexp_item);
      } else {
	UNPROTECT(1);
	Py_XDECREF(seq_object);
	return -1;
      }
    } else if (PyUnicode_Check(item)
        ) {
      new_sexp_item = StrVectorSexp_AsSexp(item);
      if (new_sexp_item) {
	SET_ELEMENT(new_sexp, ii, new_sexp_item);
      } else {
	UNPROTECT(1);
	Py_XDECREF(seq_object);
	return -1;
      }
    } else if (PyComplex_Check(item)) {
      new_sexp_item = FloatVectorSexp_AsSexp(item);
      if (new_sexp_item) {
	SET_ELEMENT(new_sexp, ii, new_sexp_item);
      } else {
	UNPROTECT(1);
	Py_XDECREF(seq_object);
	return -1;
      }
    } else {
      UNPROTECT(1);
      PyErr_Format(PyExc_ValueError,
		   "Element %zd cannot be implicitly cast to an R object.",
		   ii);
      Py_XDECREF(seq_object);
      return -1;
    }
  }
  UNPROTECT(1);
  *sexpp = new_sexp;
  Py_XDECREF(seq_object);
  return 0;
}

static int
ListVectorSexp_init(PyObject *self, PyObject *args, PyObject *kwds)
{
#ifdef RPY_VERBOSE
  printf("%p: ListVectorSexp initializing...\n", self);
#endif 
  int res = VectorSexp_init_private(self, args, kwds, 
				    (RPy_seqobjtosexpproc)RPy_SeqToVECSXP, 
				    NULL,
				    VECSXP);
#ifdef RPY_VERBOSE
  printf("done (ListVectorSexp_init).\n");
#endif 
  return res;
}

