#ifndef _RPY_PRIVATE_SEXP_H_
#define _RPY_PRIVATE_SEXP_H_

#ifndef _RPY_RINTERFACE_MODULE_
#error sexp.h should not be included directly
#endif

#include <Python.h>
#include <R.h>
#include <Rinternals.h>
#include <Rdefines.h>


static PyObject* EmbeddedR_unserialize(PyObject* self, PyObject* args);
static SEXP _rpy2_unserialize_from_char_and_size(char *raw, Py_ssize_t size);

static PyObject *rinterface_unserialize;

static PyTypeObject Sexp_Type;

#endif


