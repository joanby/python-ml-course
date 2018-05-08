#ifndef Py__RINTERFACE_H_
#define Py__RINTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <R.h>
#include <Rinternals.h>
#include <Python.h>

#if defined (__APPLE__)
#define _RPY_STRNDUP_
#endif
/* strndup is not available on solaris prior to Solaris 5.11 */
#if defined (sun) || defined (__sun)
#if defined (__SunOS_5_11)
#include <strings.h>
#else
#define _RPY_STRNDUP_
#endif
#endif

/*
 * Highest possible SEXP type (used for quick resolution of valid/invalid SEXP)
 */
#define RPY_MAX_VALIDSEXTYPE 99

/* -- SexpObject-begin -- */
typedef struct {
  Py_ssize_t pycount;
  int rcount;
  SEXP sexp;
} SexpObject;
/* -- SexpObject-end -- */

typedef struct {
  PyObject_HEAD 
  SexpObject *sObj;
  /* SEXP sexp; */
} PySexpObject;


#define RPY_COUNT(obj) (((obj)->sObj)->count)
#define RPY_SEXP(obj) (((obj)->sObj)->sexp)
  /* #define RPY_SEXP(obj) ((obj)->sexp) */
  /* #define RPY_RPYONLY(obj) (((obj)->sObj)->rpy_only) */

#define RPY_INCREF(obj) (((obj)->sObj)->count++)
  /* #define RPY_DECREF(obj) (((obj)->sObj)->count--) */


  
#define RPY_RINT_FROM_LONG(value)               \
  ((value<=(long)INT_MAX && value>=(long)INT_MIN)?(int)value:NA_INTEGER)

#define RPY_PY_FROM_RBOOL(res, rbool)                   \
  if (rbool == NA_LOGICAL) {                            \
    Py_INCREF(Py_None);                                 \
    res = Py_None;                                      \
  } else {                                              \
    res = PyBool_FromLong((long)(rbool));               \
  }

#define RPY_GIL_ENSURE(is_threaded, gstate)	\
  if (is_threaded) {				\
    gstate = PyGILState_Ensure();		\
  } 

#define RPY_GIL_RELEASE(is_threaded, gstate)    \
  if (is_threaded) {                            \
    PyGILState_Release(gstate);                 \
  }

#define RPY_PYSCALAR_TESTINT PyLong_Check

#define RPY_PYSCALAR_SETINT(py_obj)\
  RPY_RINT_FROM_LONG(PyLong_AsLong(py_obj));

#define RPY_PYSCALAR_RVECTOR(py_obj, sexp)                              \
  sexp = NULL;                                                          \
  /* The argument is not a PySexpObject, so we are going to check       \
     if conversion from a scalar type is possible */                    \
  if ((py_obj) == NACharacter_New(0)) {                                 \
    sexp = NA_STRING;                                                   \
  } else if ((py_obj) == NAInteger_New(0)) {				\
    sexp = allocVector(INTSXP, 1);					\
    PROTECT(sexp);							\
    protect_count++;                                                    \
    INTEGER_POINTER(sexp)[0] = NA_INTEGER;                              \
  } else if ((py_obj) == NALogical_New(0)) {                            \
    sexp = allocVector(LGLSXP, 1);                                      \
    PROTECT(sexp);                                                      \
    protect_count++;                                                    \
    LOGICAL_POINTER(sexp)[0] = NA_LOGICAL;                              \
  } else if ((py_obj) == NAReal_New(0)) {				\
    sexp = allocVector(REALSXP, 1);					\
    PROTECT(sexp);                                                      \
    protect_count++;                                                    \
    NUMERIC_POINTER(sexp)[0] = NA_REAL;					\
 } else if (PyBool_Check(py_obj)) {                                     \
    sexp = allocVector(LGLSXP, 1);                                      \
    LOGICAL_POINTER(sexp)[0] = py_obj == Py_True ? TRUE : FALSE;        \
    PROTECT(sexp);                                                      \
    protect_count++;                                                    \
  } else if (RPY_PYSCALAR_TESTINT(py_obj)) {				\
    sexp = allocVector(INTSXP, 1);                                      \
    INTEGER_POINTER(sexp)[0] = RPY_PYSCALAR_SETINT(py_obj);		\
    PROTECT(sexp);                                                      \
    protect_count++;                                                    \
  } else if (PyLong_Check(py_obj)) {                                    \
    sexp = allocVector(INTSXP, 1);                                      \
    INTEGER_POINTER(sexp)[0] = RPY_RINT_FROM_LONG(PyLong_AsLong(py_obj)); \
    if ((INTEGER_POINTER(sexp)[0] == -1) && PyErr_Occurred() ) {        \
      INTEGER_POINTER(sexp)[0] = NA_INTEGER;                            \
      PyErr_Clear();                                                    \
    }                                                                   \
    PROTECT(sexp);                                                      \
    protect_count++;                                                    \
 } else if (PyFloat_Check(py_obj)) {                                    \
    sexp = allocVector(REALSXP, 1);                                     \
    NUMERIC_POINTER(sexp)[0] = PyFloat_AS_DOUBLE(py_obj);               \
    PROTECT(sexp);                                                      \
    protect_count++;                                                    \
  } else if (py_obj == Py_None) {                                       \
    sexp = R_NilValue;                                                  \
  }


#define RPY_NA_TP_NEW(type_name, parent_type, pyconstructor, value)	\
  static PyObject *self = NULL;                                         \
  static char *kwlist[] = {0};                                          \
  PyObject *py_value, *new_args;					\
                                                                        \
  if (! PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist)) {          \
    return NULL;                                                        \
  }                                                                     \
                                                                        \
  if (self == NULL) {							\
    py_value = (pyconstructor)(value);				    	\
    if (py_value == NULL) {						\
      return NULL;							\
    }									\
    new_args = PyTuple_Pack(1, py_value);				\
    if (new_args == NULL) {						\
      return NULL;							\
    }									\
    self = (parent_type).tp_new(type, new_args, kwds);                  \
    Py_DECREF(new_args);						\
    if (self == NULL) {                                                 \
      return NULL;                                                      \
    }                                                                   \
  }                                                                     \
  Py_XINCREF(self);                                                     \
  return (PyObject *)self;                                              \
  

#define RPY_NA_NEW(type, type_tp_new)					\
  static PyObject *args = NULL;                                         \
  static PyObject *kwds = NULL;                                         \
  PyObject *res;                                                        \
                                                                        \
  if (args == NULL) {                                                   \
    args = PyTuple_Pack(0);                                             \
  }                                                                     \
  if (kwds == NULL) {                                                   \
    kwds = PyDict_New();                                                \
  }                                                                     \
                                                                        \
  res = (type_tp_new)(&(type), args, kwds);                             \
  if (! new) {                                                          \
    Py_DECREF(res);                                                     \
  }                                                                     \
  return res;                                                           \


  /* C API functions */
#define PyRinterface_API_NAME "rpy2.rinterface._rinterface.SEXPOBJ_C_API"
  /* -- check initialization */
#define PyRinterface_IsInitialized_NUM 0
#define PyRinterface_IsInitialized_RETURN int
#define PyRinterface_IsInitialized_PROTO (void)
  /* -- check findfun */
#define PyRinterface_FindFun_NUM 1
#define PyRinterface_FindFun_RETURN SEXP
#define PyRinterface_FindFun_PROTO (SEXP, SEXP)

  
  /* Total nmber of C API pointers */
#define PyRinterface_API_pointers 2

#ifdef _RINTERFACE_MODULE 
  /* This section is used when compiling _rinterface.c */
  static PyRinterface_IsInitialized_RETURN PyRinterface_IsInitialized PyRinterface_IsInitialized_PROTO;
  static PyRinterface_FindFun_RETURN PyRinterface_FindFun PyRinterface_FindFun_PROTO;
  static PyObject *embeddedR_isInitialized;

#else
  /* This section is used in modules that use _rinterface's API */

  static void **PyRinterface_API;

#define PyRinterface_IsInitialized \
  (*(PyRinterface_IsInitialized_RETURN (*)PyRinterface_IsInitialized_PROTO) PyRinterface_API[PyRinterface_IsInitialized_NUM])

/* Return -1 on error, 0 on success.
 * PyCapsule_Import will set an exception if there's an error.
 */
static int
import_rinterface(void)
{
  PyRinterface_API = (void **)PyCapsule_Import(PyRinterface_API_NAME, 0);
  return (PyRinterface_API != NULL) ? 0 : -1;
}
#endif



#ifdef __cplusplus
}
#endif

#endif /* !Py__RINTERFACE_H_ */

