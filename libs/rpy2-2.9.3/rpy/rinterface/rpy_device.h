#ifndef RPY_RD_H
#define RPY_RD_H
#include <R_ext/GraphicsEngine.h>
#include <R_ext/GraphicsDevice.h>
#include <Python.h>

typedef struct {
  PyObject_HEAD;
  pDevDesc grdev;
} PyGrDevObject;

#define RPY_DEV_NUM(obj) ( 1 + ndevNumber(((PyGrDevObject *)obj)->grdev) )

#define RPY_DEV_KILLED(obj) ( ((PyGrDevObject *)obj)->killed )

#define RPY_GRDEV_BOOL_GET(self, attrname)	\
  PyObject *res;							\
  if (((PyGrDevObject *)self)->grdev->attrname == TRUE) {		\
    res = Py_True;							\
  } else {								\
    res = Py_False;							\
  }									\
  Py_INCREF(res);							\
  return res								\

#define RPY_GRDEV_BOOL_SET(self, value, attrname)	\
  int res = 0;						\
  if (value == NULL) {					\
    PyErr_SetString(PyExc_TypeError,					\
		  "The attribute "#attrname"cannot be deleted");	\
    res = -1;								\
  } else if (! PyBool_Check(value)) {					\
    PyErr_SetString(PyExc_TypeError,					\
		    "The attribute "#attrname" must be a boolean");	\
    res = -1;								\
  } else if (value == Py_True) {					\
    ((PyGrDevObject *)self)->grdev->attrname = TRUE;			\
  } else if (value == Py_False) {					\
    ((PyGrDevObject *)self)->grdev->attrname = FALSE;			\
  } else {								\
    PyErr_SetString(PyExc_TypeError,					\
		    "Mysterious error when setting the attribute "#attrname"."); \
    res = -1;								\
  }									\
  return res								

#define RPY_GRDEV_BOOL_GETSET(attrname, docstring)		\
  PyDoc_STRVAR(GrDev_##attrname##_doc,				\
	       docstring);					\
  static PyObject*						\
  GrDev_##attrname##_get(PyObject *self)			\
  {								\
    RPY_GRDEV_BOOL_GET(self, attrname);				\
					 }			\
  static int							\
  GrDev_##attrname##_set(PyObject *self, PyObject *value)	\
  {								\
    RPY_GRDEV_BOOL_SET(self, value, attrname);			\
  }								


#define RPY_GRDEV_FLOAT_SET(self, value, attrname)	\
  int res = 0;						\
  if (value == NULL) {					\
    PyErr_SetString(PyExc_TypeError,					\
		    "The attribute '"#attrname"' cannot be deleted");	\
    res = -1;								\
  } else if (! PyFloat_Check(value)) {					\
    PyErr_SetString(PyExc_TypeError,					\
		    "The attribute '"#attrname"' must be a float");	\
    res = -1;								\
  } else {								\
  ((PyGrDevObject *)self)->grdev->attrname = PyFloat_AsDouble(value);	\
  }									\
  return res								

SEXP rpy_devoff(SEXP devnum, SEXP rho);

#endif /* !RPY_RD_H */
