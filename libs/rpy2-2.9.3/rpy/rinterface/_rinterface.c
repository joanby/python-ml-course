/* A python-R interface*/

/* 
 * The authors for the original RPy code, as well as
 * belopolsky for his contributed code, are listed here as authors;
 * although the design is largely new, parts of this code is
 * derived from their contributions. 
 * 
 * Laurent Gautier - 2008
 */

/*
 ***** BEGIN LICENSE BLOCK *****
 * Version: GPLv2+
 *
 * 
 * Copyright (C) 2008-2016 Laurent Gautier
 *
 * Portions created by Alexander Belopolsky are 
 * Copyright (C) 2006 Alexander Belopolsky.
 *
 * Portions created by Gregory R. Warnes are
 * Copyright (C) 2003-2008 Gregory Warnes.
 *
 * Portions created by Walter Moreira are 
 * Copyright (C) 2002-2003 Walter Moreira
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
 *
 * ***** END LICENSE BLOCK ***** */

#include <signal.h>
#include <setjmp.h>

#define PY_SSIZE_T_CLEAN 
#include "Python.h"

#define _RINTERFACE_MODULE
#include "_rinterface.h"

#if defined(Win32) || defined(Win64)
#include <winsock2.h>
#endif

#include <R.h>
#include <Rversion.h>
#include <Rinternals.h>
#include <Rdefines.h>

#if !(defined(Win32) || defined(Win64))
#include <Rinterface.h>
#endif
#include <R_ext/Complex.h>
#include <Rembedded.h>


#include <R_ext/eventloop.h>

#include <R_ext/Parse.h>
#include <R_ext/Rdynload.h>
#include <R_ext/RStartup.h>

/*FIXME:  required to fix the R issue with setting static char* values
 for readline variable (making Python's readline crash when trying to free them) */

#ifdef HAS_READLINE
#include <readline/readline.h>
#endif

/* From Defn.h */
#ifdef HAVE_POSIX_SETJMP
#define SIGJMP_BUF sigjmp_buf
#else
#define SIGJMP_BUF jmp_buf
#endif

#define _RPY_RINTERFACE_MODULE_

static PyObject* EmbeddedR_unserialize(PyObject* self, PyObject* args);

#include "embeddedr.h"
#include "na_values.h"
#include "sexp.h"
#include "r_utils.h"
#include "buffer.h"
#include "array.h"
#include "sequence.h"
#include "rexternalptr.h"

static PySexpObject* newPySexpObject(const SEXP sexp);
/* Helper variable to quickly resolve SEXP types.
 * An array of strings giving either
 * the SEXP name (INTSXP, REALSXP, etc...), or a NULL
 * if there is no such valid SEXP.
 */
static char **validSexpType;
static SEXP newSEXP(PyObject *object, const int rType);

#include "embeddedr.c"
#include "null_value.c"
#include "na_values.c"
#include "sexp.c"
#include "buffer.c"
#include "array.c"
#include "sequence.c"
#include "rexternalptr.c"

/* A tuple that holds options to initialize R */
static PyObject *initOptions;

static SEXP errMessage_SEXP;
static PyObject *RPyExc_RuntimeError = NULL;
static PyObject *RPyExc_ParsingError = NULL;
static PyObject *RPyExc_ParsingIncompleteError = NULL;

#if (defined(Win32) || defined(Win64))
/* R instance as a global */
Rstart Rp;
#endif

/* FIXME: see the details of interruption */
/* Indicates whether the R interpreter was interrupted by a SIGINT */
int interrupted = 0;
/* Abort the current R computation due to a SIGINT */
static void
interrupt_R(int signum)
{
  printf("-->interrupted.\n");
  interrupted = 1;
  error("Interrupted");
}


SIGJMP_BUF env_sigjmp;
/* Python's signal handler */
static PyOS_sighandler_t python_sighandler, last_sighandler;

/* In SAGE, explicit defintions */
/* /\* Python handler (definition varies across platforms) *\/ */
/* #if defined(__CYGWIN32__) /\* Windows XP *\/ */
/*   _sig_func_ptr python_sighandler; */
/* #elif defined(__FreeBSD__) /\* FreeBSD *\/ */
/*   sig_t python_sighandler; */
/* #elif defined(__APPLE__) /\* OSX *\/ */
/*   sig_t python_sighandler; */
/* #elif defined (__sun__) || defined (__sun) /\* Solaris *\/ */
/*   __sighandler_t python_sighandler; */
/* #else /\* Other, e.g., Linux *\/ */
/*   __sighandler_t python_sighandler; */
/* #endif   */


#if defined(_RPY_STRNDUP_) /* OSX 10.5 and 10.6, and older BSD */
inline char* strndup (const char *s, size_t n)
{
  size_t len = strlen (s);
  char *ret;
  
  if (len <= n)
    return strdup (s);
  
  ret = malloc(n + 1);
  strncpy(ret, s, n);
  ret[n] = '\0';
  return ret;
} 
#endif


PyDoc_STRVAR(module_doc,
             "Low-level functions to interface with R.\n\
 One should mostly consider calling the functions defined here when\
 writing a higher level interface between python and R.\
 Check the documentation for the module this is bundled into if\
 you only wish to have an off-the-shelf interface with R.\
\n\
");

static PyObject *RPY_R_VERSION_BUILD;
static PySexpObject *globalEnv;
static PySexpObject *baseNameSpaceEnv;
static PySexpObject *emptyEnv;
static PySexpObject *rpy_R_NilValue;

#ifdef RPY_DEBUG_PRESERVE
static int preserved_robjects = 0;
#endif


/* NAs */
static PyObject* NAInteger_New(int new);
static PyTypeObject NAInteger_Type;

static PyObject* NALogical_New(int new);
static PyTypeObject NALogical_Type;

static PyObject* NAReal_New(int new);
static PyTypeObject NAReal_Type;

static PyObject* NAComplex_New(int new);
static PyTypeObject NAComplex_Type;

static PyObject* NACharacter_New(int new);
static PyTypeObject NACharacter_Type;

/* type tag for Python external methods */
static PySexpObject *R_PyObject_type_tag;
static void RegisterExternalSymbols(void);

/* --- set output from the R console ---*/

static inline PyObject* EmbeddedR_setAnyCallback(PyObject *self,
                                                 PyObject *args,
                                                 PyObject **target)
{
  
  PyObject *result;
  PyObject *function;
  
  if ( PyArg_ParseTuple(args, "O:console", 
                        &function)) {
    
    if (function != Py_None && !PyCallable_Check(function)) {
      PyErr_SetString(PyExc_TypeError, "parameter must be callable");
      return NULL;
    }

    Py_XDECREF(*target);
    if (function == Py_None) {
      *target = NULL;
    } else {
      Py_XINCREF(function);
      *target = function;
    }
    Py_INCREF(Py_None);
    result = Py_None;
  } else {
    PyErr_SetString(PyExc_TypeError, "The parameter should be a callable.");
    return NULL;
  }
  return result;
  
}

static PyObject* EmbeddedR_getAnyCallback(PyObject *self,
                                                 PyObject *args,
                                                 PyObject *target)
{
  PyObject *result = NULL;

  if (PyArg_ParseTuple(args, "")) {
    if (target == NULL) {
      result = Py_None;
    } else {
      result = target;
    }
  } else {

  }
  Py_XINCREF(result);
  return result;
}


static PyObject* writeConsoleRegularCallback = NULL;

static PyObject* EmbeddedR_setWriteConsoleRegular(PyObject *self,
						  PyObject *args)
{
  PyObject *res = EmbeddedR_setAnyCallback(self, args, &writeConsoleRegularCallback);
  return res;
}

PyDoc_STRVAR(EmbeddedR_setWriteConsoleRegular_doc,
             "set_writeconsole_regular(f)\n\n"
             "Set how to handle regular output from the R console with either None"
             " or a function f such as f(output) returns None"
             " (f only has side effects).");

static PyObject* writeConsoleWarnErrorCallback = NULL;

static PyObject* EmbeddedR_setWriteConsoleWarnError(PyObject *self,
						    PyObject *args)
{
  PyObject *res = EmbeddedR_setAnyCallback(self, args, &writeConsoleWarnErrorCallback);
  return res;
}

PyDoc_STRVAR(EmbeddedR_setWriteConsoleWarnError_doc,
             "set_writeconsole_warnerror(f)\n\n"
             "Set how to handle warning or error output from the R console"
	     " with either None"
             " or a function f such as f(output) returns None"
             " (f only has side effects).");

static PyObject * EmbeddedR_getWriteConsoleRegular(PyObject *self,
						   PyObject *args)
{
  return EmbeddedR_getAnyCallback(self, args, writeConsoleRegularCallback);
}

PyDoc_STRVAR(EmbeddedR_getWriteConsoleRegular_doc,
             "get_writeconsole_regular()\n\n"
             "Retrieve the current R console output handler"
             " (see set_writeconsole_regular)");

static PyObject * EmbeddedR_getWriteConsoleWarnError(PyObject *self,
						     PyObject *args)
{
  return EmbeddedR_getAnyCallback(self, args, writeConsoleWarnErrorCallback);
}

PyDoc_STRVAR(EmbeddedR_getWriteConsoleWarnError_doc,
             "get_writeconsole_warnerror()\n\n"
             "Retrieve the current R console output handler for warnings and errors."
             " (see set_writeconsole_warnerror)");

static void
EmbeddedR_WriteConsoleEx(const char *buf, int len, int otype)
{
  /* otype can be 0: regular output or 1: error or warning */
  void *consolecallback = NULL;
  switch(otype) {
  case 0:
    consolecallback = writeConsoleRegularCallback;
    break;
  case 1:
    consolecallback = writeConsoleWarnErrorCallback;
    break;
  default:
    printf("unknown otype in EmbeddedR_WriteConsoleEx.\n");   
  } 

  PyObject *arglist;
  PyObject *result;
  const int is_threaded = PyEval_ThreadsInitialized();
  PyGILState_STATE gstate;
  RPY_GIL_ENSURE(is_threaded, gstate);

  /* It is necessary to restore the Python handler when using a Python
     function for I/O. */

  PyOS_setsig(SIGINT, python_sighandler);

  arglist = Py_BuildValue("(s)", buf);

  if (! arglist) {    PyErr_NoMemory();
/*     signal(SIGINT, old_int); */
/*     return NULL; */
  }

  if (consolecallback == NULL) {
    return;
  }

  result = PyEval_CallObject(consolecallback, arglist);
  PyObject* pythonerror = PyErr_Occurred();
  if (pythonerror != NULL) {
    /* All R actions should be stopped since the Python callback failed,
     and the Python exception raised up.*/
    /* FIXME: Print the exception in the meanwhile */
    PyErr_Print();
    PyErr_Clear();
  }

  Py_XDECREF(arglist);
/*   signal(SIGINT, old_int); */
  
  Py_XDECREF(result);  
  RPY_GIL_RELEASE(is_threaded, gstate);
}


static PyObject* showMessageCallback = NULL;

static PyObject* EmbeddedR_setShowMessage(PyObject *self,
                                          PyObject *args)
{
  return EmbeddedR_setAnyCallback(self, args, &showMessageCallback);  
}

PyDoc_STRVAR(EmbeddedR_setShowMessage_doc,
             "set_showmessage(f)\n\n"
             "Set how to handle alert message from R with either None"
             " or a function f such as f(message) returns None"
             " (f only has side effects).");

static PyObject * EmbeddedR_getShowMessage(PyObject *self,
                                            PyObject *args)
{
  return EmbeddedR_getAnyCallback(self, args, showMessageCallback);
}

PyDoc_STRVAR(EmbeddedR_getShowMessage_doc,
             "get_showmessage()\n\n"
             "Retrieve the current R alert message handler"
             " (see set_showmessage)");


static void
EmbeddedR_ShowMessage(const char *buf)
{
  PyOS_sighandler_t old_int;
  PyObject *arglist;
  PyObject *result;

  const int is_threaded = PyEval_ThreadsInitialized();
  PyGILState_STATE gstate;
  RPY_GIL_ENSURE(is_threaded, gstate);

  /* It is necessary to restore the Python handler when using a Python
     function for I/O. */
  old_int = PyOS_getsig(SIGINT);
  PyOS_setsig(SIGINT, python_sighandler);
  arglist = Py_BuildValue("(s)", buf);
  if (! arglist) {
    //PyErr_NoMemory();
    printf("Ouch. Likely a out of memory.\n");
    signal(SIGINT, old_int);
    return;
  }

  if (showMessageCallback == NULL) {
    return;
  }

  result = PyEval_CallObject(showMessageCallback, arglist);
  PyObject* pythonerror = PyErr_Occurred();
  if (pythonerror != NULL) {
    /* All R actions should be stopped since the Python callback failed,
     and the Python exception raised up.*/
    /* FIXME: Print the exception in the meanwhile */
    PyErr_Print();
    PyErr_Clear();
  }

  Py_DECREF(arglist);
/*   signal(SIGINT, old_int); */
  
  Py_XDECREF(result);
  RPY_GIL_RELEASE(is_threaded, gstate);
}

static PyObject* readConsoleCallback = NULL;

static PyObject* EmbeddedR_setReadConsole(PyObject *self,
                                          PyObject *args)
{
  return EmbeddedR_setAnyCallback(self, args, &readConsoleCallback); 
}

PyDoc_STRVAR(EmbeddedR_setReadConsole_doc,
             "set_readconsole(f)\n\n"
             "Set how to handle input to R with either None"
             " or a function f such as f(prompt) returns the string"
             " message to be passed to R");

static PyObject * EmbeddedR_getReadConsole(PyObject *self,
                                            PyObject *args)
{
  return EmbeddedR_getAnyCallback(self, args, readConsoleCallback);
}

PyDoc_STRVAR(EmbeddedR_getReadConsole_doc,
             "get_readconsole()\n\n"
             "Retrieve the current R alert message handler"
             " (see set_readconsole)");

static int
EmbeddedR_ReadConsole(const char *prompt, unsigned char *buf, 
                      int len, int addtohistory)
{
  PyObject *arglist;
  PyObject *result;

  const int is_threaded = PyEval_ThreadsInitialized();
  PyGILState_STATE gstate;
  RPY_GIL_ENSURE(is_threaded, gstate);

  /* It is necessary to restore the Python handler when using a Python
     function for I/O. */
/*   old_int = PyOS_getsig(SIGINT); */
/*   PyOS_setsig(SIGINT, python_sighandler); */
  arglist = Py_BuildValue("(s)", prompt);
  if (! arglist) {
    PyErr_NoMemory();
/*     signal(SIGINT, old_int); */
/* return NULL; */
  }

  if (readConsoleCallback == NULL) {
    Py_DECREF(arglist);
    RPY_GIL_RELEASE(is_threaded, gstate);
    return -1;
  }

  #ifdef RPY_DEBUG_CONSOLE
  printf("Callback for console input...");
  #endif
  result = PyEval_CallObject(readConsoleCallback, arglist);
  #ifdef RPY_DEBUG_CONSOLE
  printf("done.(%p)\n", result);
  #endif

  Py_XDECREF(arglist);

  PyObject* pythonerror = PyErr_Occurred();
  if (pythonerror != NULL) {
    /* All R actions should be stopped since the Python callback failed,
     and the Python exception raised up.*/
    /* FIXME: Print the exception in the meanwhile */
    PyErr_Print();
    PyErr_Clear();
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }


  if (result == NULL) {
    /* FIXME: can this be reached ? result == NULL while no error ? */
/*     signal(SIGINT, old_int); */
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }

  const char *input_str = NULL;

  int is_unicode = PyUnicode_Check(result);
  PyObject *pybytes = NULL;
  if (is_unicode) {
      pybytes = PyUnicode_AsUTF8String(result);
      input_str = PyBytes_AsString(pybytes);
  } else if (PyBytes_Check(result)) {
    input_str = PyBytes_AsString(result);
  } else {
    PyErr_Format(PyExc_ValueError, \
		 "The R console callback must return a unicode string or bytes.");
    PyErr_Print();
    PyErr_Clear();
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }

  //const char *input_str = PyBytes_AsString(result);
  if (! input_str) {
    if (is_unicode)
      Py_XDECREF(pybytes);
    PyErr_Print();
    PyErr_Clear();
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }

  /* Snatched from Rcallbacks.c in JRI */
  int l=strlen(input_str);
  strncpy((char *)buf, input_str, (l>len-1)?len-1:l);
  buf[(l>len-1)?len-1:l]=0;
  /* --- */

  if (is_unicode)
    Py_XDECREF(pybytes);
  
  Py_XDECREF(result);
/*   signal(SIGINT, old_int); */

  RPY_GIL_RELEASE(is_threaded, gstate);
  return 1;
}

static PyObject* flushConsoleCallback = NULL;

static PyObject* resetConsoleCallback = NULL;

static PyObject* EmbeddedR_setFlushConsole(PyObject *self,
                                           PyObject *args)
{
  return EmbeddedR_setAnyCallback(self, args, &flushConsoleCallback);  
}

PyDoc_STRVAR(EmbeddedR_setFlushConsole_doc,
             "set_flushconsole(f)\n\n"
             "Set how to handle the flushing of the R console with either None"
             " or a function f such as f() returns None"
             " (f only has side effects).");

static PyObject* EmbeddedR_setResetConsole(PyObject *self,
                                           PyObject *args)
{
  return EmbeddedR_setAnyCallback(self, args, &resetConsoleCallback);  
}

PyDoc_STRVAR(EmbeddedR_setResetConsole_doc,
             "set_resetconsole(f)\n\n"
             "Set how to handle the reset R console with either None"
             " or a function f such as f() returns None"
             " (f only has side effects).");

static PyObject * EmbeddedR_getFlushConsole(PyObject *self,
                                            PyObject *args)
{
  return EmbeddedR_getAnyCallback(self, args, flushConsoleCallback);
}

PyDoc_STRVAR(EmbeddedR_getFlushConsole_doc,
             "get_flushconsole()\n\n"
             "Retrieve the current R handler to flush the console"
             " (see set_flushconsole)");

static void
EmbeddedR_FlushConsole(void)
{

  const int is_threaded = PyEval_ThreadsInitialized();
  PyGILState_STATE gstate;
  RPY_GIL_ENSURE(is_threaded, gstate);

  //PyObject *result (returned by call below);
  PyEval_CallObject(flushConsoleCallback, NULL);
  PyObject* pythonerror = PyErr_Occurred();
  if (pythonerror != NULL) {
    /* All R actions should be stopped since the Python callback failed,
     and the Python exception raised up.*/
    /* FIXME: Print the exception in the meanwhile */
    PyErr_Print();
    PyErr_Clear();
  }

  RPY_GIL_RELEASE(is_threaded, gstate);
  return;
}

static PyObject * EmbeddedR_getResetConsole(PyObject *self,
                                            PyObject *args)
{
  return EmbeddedR_getAnyCallback(self, args, resetConsoleCallback);
}

PyDoc_STRVAR(EmbeddedR_getResetConsole_doc,
             "get_resetconsole()\n\n"
             "Retrieve the current R handler to reset the console"
             " (see set_resetconsole)");

static void
EmbeddedR_ResetConsole(void)
{

  const int is_threaded = PyEval_ThreadsInitialized();
  PyGILState_STATE gstate;
  RPY_GIL_ENSURE(is_threaded, gstate);

  //PyObject *result (returned by call below);
  if (resetConsoleCallback != NULL) {
    PyEval_CallObject(resetConsoleCallback, NULL);
    PyObject* pythonerror = PyErr_Occurred();
    if (pythonerror != NULL) {
      /* All R actions should be stopped since the Python callback failed,
	 and the Python exception raised up.*/
      /* FIXME: Print the exception in the meanwhile */
      PyErr_Print();
      PyErr_Clear();
    }
  }
  RPY_GIL_RELEASE(is_threaded, gstate);
  return;
}

static PyObject* chooseFileCallback = NULL;

static PyObject* EmbeddedR_setChooseFile(PyObject *self,
                                         PyObject *args)
{
  return EmbeddedR_setAnyCallback(self, args, &chooseFileCallback);  
}

PyDoc_STRVAR(EmbeddedR_setChooseFile_doc,
             "Use the function to handle R's requests for choosing a file.");

static PyObject * EmbeddedR_getChooseFile(PyObject *self,
                                            PyObject *args)
{
  return EmbeddedR_getAnyCallback(self, args, chooseFileCallback);
}

PyDoc_STRVAR(EmbeddedR_getChooseFile_doc,
             "Retrieve current R console output handler (see setChooseFile).");

/* Callback to replace R's default function for choosing a file
   This return 1 on success, 0 on failure.
   In the case of failure the calling function will fail as 
*/
static int
EmbeddedR_ChooseFile(int new, char *buf, int len)
{
  PyObject *arglist;
  PyObject *result;

  const int is_threaded = PyEval_ThreadsInitialized();
  PyGILState_STATE gstate;
  RPY_GIL_ENSURE(is_threaded, gstate);

  arglist = Py_BuildValue("(y)", buf);
  if (! arglist) {
    PyErr_NoMemory();
  }

  if (chooseFileCallback == NULL) {
    Py_DECREF(arglist);
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }

  result = PyEval_CallObject(chooseFileCallback, arglist);

  PyObject* pythonerror = PyErr_Occurred();
  if (pythonerror != NULL) {
    /* All R actions should be stopped since the Python callback failed,
     and the Python exception raised up.*/
    /* FIXME: Print the exception in the meanwhile */
    PyErr_Print();
    PyErr_Clear();
    Py_XDECREF(arglist);
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }
  
  if (result == NULL) {
    /* FIXME: can this be reached ? result == NULL while no error ? */
    printf("Error: trouble with chooseFileCallback, we should not be here.\n");
    Py_XDECREF(arglist);
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }
  PyObject *pybytes = PyUnicode_AsUTF8String(result);
  char *path_str = PyBytes_AsString(pybytes);
  //char *path_str = PyBytes_AsString(result);

  if (! path_str) {
    Py_DECREF(pybytes);
    Py_DECREF(result);
    PyErr_SetString(PyExc_TypeError, 
                    "Returned value should have a string representation");
    PyErr_Print();
    PyErr_Clear();
    Py_DECREF(arglist);
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }

  /* As shown in gnomeGUI */
  int l=strlen(path_str);
  strncpy((char *)buf, path_str, (l>len-1)?len-1:l);
  buf[(l>len-1)?len-1:l] = '\0';
  /* --- */

  Py_DECREF(arglist);  
  Py_DECREF(result);

  RPY_GIL_RELEASE(is_threaded, gstate);
  return l;
}

static PyObject* showFilesCallback = NULL;

static PyObject* EmbeddedR_setShowFiles(PyObject *self,
                                        PyObject *args)
{
  return EmbeddedR_setAnyCallback(self, args, &showFilesCallback);  
}

PyDoc_STRVAR(EmbeddedR_setShowFiles_doc,
             "Use the function to display files.");

static PyObject * EmbeddedR_getShowFiles(PyObject *self,
                                            PyObject *args)
{
  return EmbeddedR_getAnyCallback(self, args, showFilesCallback);
}

PyDoc_STRVAR(EmbeddedR_getShowFiles_doc,
             "Retrieve current R console output handler (see setShowFiles).");

static int
EmbeddedR_ShowFiles(int nfile, const char **file, const char **headers,
                    const char *wtitle, Rboolean del, const char *pager)
{

  const int is_threaded = PyEval_ThreadsInitialized();
  PyGILState_STATE gstate;

  RPY_GIL_ENSURE(is_threaded, gstate);

  if (showFilesCallback == NULL) {
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }
  
  if (nfile < 1) {
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }

  PyObject *arglist;
  PyObject *result;

  PyObject *py_del;
  RPY_PY_FROM_RBOOL(py_del, del);
   PyObject *py_wtitle = PyUnicode_FromString(wtitle);
   PyObject *py_pager = PyUnicode_FromString(pager);
  PyObject *py_fileheaders_tuple = PyTuple_New(nfile);
  PyObject *py_fileheader;
  int f_i;
  for (f_i = 0; f_i < nfile; f_i++) {
    py_fileheader = PyTuple_New(2);
    if (PyTuple_SetItem(py_fileheader, 0,
                        PyUnicode_FromString(headers[f_i])) != 0) {
      Py_DECREF(py_fileheaders_tuple);
      /*FIXME: decref other PyObject arguments */
      RPY_GIL_RELEASE(is_threaded, gstate);
      return 0;
    }
    if (PyTuple_SetItem(py_fileheader, 1,
                        PyUnicode_FromString(file[f_i])) != 0) {
      Py_DECREF(py_fileheaders_tuple);
      /*FIXME: decref other PyObject arguments */
      RPY_GIL_RELEASE(is_threaded, gstate);
      return 0;
    }
    if (PyTuple_SetItem(py_fileheaders_tuple, f_i,
                        py_fileheader) != 0) {
      Py_DECREF(py_fileheaders_tuple);
      /*FIXME: decref other PyObject arguments */
      RPY_GIL_RELEASE(is_threaded, gstate);
      return 0;
    }
  }

  arglist = Py_BuildValue("OOOO", py_fileheaders_tuple, py_wtitle, py_del, py_pager);
  if (! arglist) {
    PyErr_Print();
    PyErr_NoMemory();
    /* FIXME: decref PyObject arguments ? */
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }

  result = PyEval_CallObject(showFilesCallback, arglist);

  if (PyErr_Occurred()) {
    /* All R actions should be stopped since the Python callback failed,
     and the Python exception raised up.*/
    /* FIXME: Print the exception in the meanwhile */
    PyErr_Print();
    PyErr_Clear();
    Py_XDECREF(arglist);
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }
  
  if (result == NULL) {
    /* FIXME: can this be reached ? result == NULL while no error ? */
    printf("Error: trouble with chooseFileCallback, we should not be here.\n");
    Py_XDECREF(arglist);
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }

  /*FIXME: check that nothing is returned ? */
  if (! 1) {
    Py_DECREF(result);
    PyErr_SetString(PyExc_TypeError, 
                    "Returned value should be None");
    PyErr_Print();
    PyErr_Clear();
    Py_DECREF(arglist);
    RPY_GIL_RELEASE(is_threaded, gstate);
    return 0;
  }

  Py_DECREF(arglist);
  Py_DECREF(result);
  RPY_GIL_RELEASE(is_threaded, gstate);
  return 1;
}

static PyObject* cleanUpCallback = NULL;
static PyObject* EmbeddedR_setCleanUp(PyObject *self,
                                       PyObject *args)
{
  return EmbeddedR_setAnyCallback(self, args, &cleanUpCallback);  
}

PyDoc_STRVAR(EmbeddedR_setCleanUp_doc,
             "Set the function called to clean up when exiting R.");

static PyObject * EmbeddedR_getCleanUp(PyObject *self,
                                       PyObject *args)
{
  PyObject* res = EmbeddedR_getAnyCallback(self, args, cleanUpCallback);
  return res;
}

PyDoc_STRVAR(EmbeddedR_getCleanUp_doc,
             "Get the function called to clean up when exiting R.");

extern SA_TYPE SaveAction; 
static void
EmbeddedR_CleanUp(SA_TYPE saveact, int status, int runLast)
{
  /*
    R_CleanUp is invoked at the end of the session to give the user the
    option of saving their data.
    If ask == SA_SAVEASK the user should be asked if possible (and this
    option should not occur in non-interactive use).
    If ask = SA_SAVE or SA_NOSAVE the decision is known.
    If ask = SA_DEFAULT use the SaveAction set at startup.
    In all these cases run .Last() unless quitting is cancelled.
    If ask = SA_SUICIDE, no save, no .Last, possibly other things.
  */

  const int is_threaded = PyEval_ThreadsInitialized();
  PyGILState_STATE gstate;

  if(saveact == SA_DEFAULT) { /* The normal case apart from R_Suicide */
    saveact = SaveAction;
  }
  
  RPY_GIL_ENSURE(is_threaded, gstate);
  
  PyObject *arglist = Py_BuildValue("iii", saveact, status, runLast);
  PyObject *result = PyEval_CallObject(cleanUpCallback, arglist);
  PyObject* pythonerror = PyErr_Occurred();
  
  if (pythonerror != NULL) {
    /* All R actions should be stopped since the Python callback failed,
             and the Python exception raised up.*/
    /* FIXME: Print the exception in the meanwhile */
    PyErr_Print();
    PyErr_Clear();
  } else {
    if (result == Py_None)
      jump_to_toplevel();

    int res_true = PyObject_IsTrue(result);
    switch(res_true) {
    case -1:
      printf("*** error while testing of the value returned from the cleanup callback is true.\n");
      jump_to_toplevel();
      break;
    case 1:
      saveact = SA_SAVE;
      break;
    case 0:
      saveact = SA_NOSAVE;
      break;
    }
    Py_XDECREF(arglist);
    RPY_GIL_RELEASE(is_threaded, gstate);
  }

  if (saveact == SA_SAVEASK) {
#if ! (defined(Win32) || defined(Win64))
    if (R_Interactive) {
#endif
      /* if (cleanUpCallback != NULL) {  */
        
      /*        } */
      /* } else { */
        saveact = SaveAction;
      /* } */
#if ! (defined(Win32) || defined(Win64))
    } else {
        saveact = SaveAction;
    }
#endif
  }
  switch (saveact) {
  case SA_SAVE:
    if(runLast) R_dot_Last();
    if(R_DirtyImage) R_SaveGlobalEnv();
/*     if (CharacterMode == RGui) { */
/*       R_setupHistory(); /\* re-read the history size and filename *\/ */
/*       wgl_savehistory(R_HistoryFile, R_HistorySize); */
/*     } else if(R_Interactive && CharacterMode == RTerm) { */
/*       R_setupHistory(); /\* re-read the history size and filename *\/ */
/*       gl_savehistory(R_HistoryFile, R_HistorySize); */
/*     } */
    break;
  case SA_NOSAVE:
    if(runLast) R_dot_Last();
    break;
  case SA_SUICIDE:
  default:
    break;
  }
  R_RunExitFinalizers();
/*   editorcleanall(); */
/*   CleanEd(); */
  R_CleanTempDir();
  Rf_KillAllDevices();
/*   AllDevicesKilled = TRUE; */
/*   if (R_Interactive && CharacterMode == RTerm) */
/*     SetConsoleTitle(oldtitle); */
/*   if (R_CollectWarnings && saveact != SA_SUICIDE */
/*       && CharacterMode == RTerm) */
/*     PrintWarnings(); */
/*   app_cleanup(); */
/*   RConsole = NULL; */
/*   if(ifp) fclose(ifp); */
/*   if(ifile[0]) unlink(ifile); */
  /* exit(status); */
  
}

/* --- Initialize and terminate an embedded R --- */
static PyObject* EmbeddedR_getinitoptions(PyObject *self) 
{
  return initOptions;
}
PyDoc_STRVAR(EmbeddedR_get_initoptions_doc,
             "\
Get the options used to initialize R.\
");

static PyObject* EmbeddedR_setinitoptions(PyObject *self, PyObject *tuple) 
{

  if (rpy_has_status(RPY_R_INITIALIZED)) {
    PyErr_Format(PyExc_RuntimeError, 
                 "Options cannot be set once R has been initialized.");
    return NULL;
  }

  int istuple = PyTuple_Check(tuple);
  if (! istuple) {
    PyErr_Format(PyExc_ValueError, "Parameter should be a tuple.");
    return NULL;
  }

  /* now test that all elements of the tuple are strings (Python2)
   * or bytes (Python3).
   */
  Py_ssize_t ii;
  for (ii = 0; ii < PyTuple_GET_SIZE(tuple); ii++) {
    if (! PyBytes_Check(PyTuple_GET_ITEM(tuple, ii))) {
      PyErr_Format(PyExc_ValueError, "All options should be bytes.");
      return NULL;
    }
  }
  
  
  Py_DECREF(initOptions);
  Py_INCREF(tuple);
  initOptions = tuple;
  Py_INCREF(Py_None);
  return Py_None;
}
PyDoc_STRVAR(EmbeddedR_set_initoptions_doc,
             "\
Set the options used to initialize R.\
");


/* --- R_ProcessEvents ---*/

static PyObject* EmbeddedR_ProcessEvents(PyObject *self)
{
  if (! (rpy_has_status(RPY_R_INITIALIZED))) {
    PyErr_Format(PyExc_RuntimeError, 
                 "R should not process events before being initialized.");
    return NULL;
  }
  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return NULL;
  }
  embeddedR_setlock();
#if defined(HAVE_AQUA) || (defined(Win32) || defined(Win64))
  /* Can the call to R_ProcessEvents somehow fail ? */
  R_ProcessEvents();
#endif
#if ! (defined(Win32) || defined(Win64))
  R_runHandlers(R_InputHandlers, R_checkActivity(0, 1));
#endif
  embeddedR_freelock();
  Py_INCREF(Py_None);
  return Py_None;
}
PyDoc_STRVAR(EmbeddedR_ProcessEvents_doc,
             "Process R events. This function is a simple wrapper around\n"
             "R_ProcessEvents (on win32 and MacOS X-Aqua)\n"
             "and R_runHandlers (on other platforms).");


#if defined(Win32) || defined(Win64)
void win32CallBack()
{
   /* called during i/o, eval, graphics in ProcessEvents */
}
void Re_Busy(int which)
{
  
}
#endif

static void
end_r(void)
{
  /* taken from the tests/Embedded/shutdown.c in the R source tree */

  R_dot_Last();           
  R_RunExitFinalizers();  
  /* CleanEd(); */
  Rf_KillAllDevices();
  
  R_CleanTempDir();
  /* PrintWarnings(); */
  R_gc();
  /* */

  /*NOTE: This is only part of the procedure to terminate
   R - more in EmbeddedR_end()*/


}


PyDoc_STRVAR(EmbeddedR_init_doc,
             "Initialize an embedded R.\n"
	     "initr(r_preservehash=False) -> return code (an integer)\n"
	     "\nThe optional argument r_preservehash is using an hash of "
	     "the memory address as a key in an R environment to "
	     "preserve R objects from garbage collection.");

static PyObject* EmbeddedR_init(PyObject *self, PyObject *args, PyObject *kwds)
{

  static int status;
  
  if (rpy_has_status(RPY_R_INITIALIZED)) {
    return PyLong_FromLong(status);
/*     PyErr_Format(PyExc_RuntimeError, "R can only be initialized once."); */
/*     return NULL; */
  }


  PyObject *preservehash = Py_False;
  static char *kwlist[] = {"r_preservehash", NULL};
  if (! PyArg_ParseTupleAndKeywords(args, kwds, "|O!", 
                                    kwlist,
                                    &PyBool_Type, &preservehash) ){
    return NULL;
  }

  const Py_ssize_t n_args = PySequence_Size(initOptions);
  char *options[n_args];

  PyObject *opt_string;
  Py_ssize_t ii;
  for (ii = 0; ii < n_args; ii++) {
    opt_string = PyTuple_GetItem(initOptions, ii);
    options[ii] = PyBytes_AsString(opt_string);
  }


#if ! (defined(Win32) || defined(Win64))

#else
  /* --- Win32 --- */
  structRstart rp;
  Rp = &rp;

  char RHome[260];
  char RUser[260];

  R_setStartTime();
  R_DefParams(Rp);
  if (getenv("R_HOME")) {
    strcpy(RHome, getenv("R_HOME"));
  } else {
    PyErr_Format(PyExc_RuntimeError, "R_HOME not defined.");
    return NULL;
  }
  Rp->rhome = RHome;

  if (getenv("R_USER")) {
    strcpy(RUser, getenv("R_USER"));
  } else if (getenv("HOME")) {
    strcpy(RUser, getenv("HOME"));
  } else if (getenv("HOMEDIR")) {
    strcpy(RUser, getenv("HOMEDIR"));
    strcat(RUser, getenv("HOMEPATH"));
  } else {
    PyErr_Format(PyExc_RuntimeError, "R_USER not defined.");
    return NULL;
  }
  Rp->home = RUser;
  /* Rp->CharacterMode = LinkDLL; */
  Rp->ReadConsole = EmbeddedR_ReadConsole;
  Rp->WriteConsole = NULL; /* Force use of WriteConsoleEx */
  Rp->WriteConsoleEx = EmbeddedR_WriteConsoleEx;

  Rp->Busy = Re_Busy;
  Rp->ShowMessage = EmbeddedR_ShowMessage;
  /* Rp->FlushConsole = EmbeddedR_FlushConsole; */
  Rp->ResetConsole = EmbeddedR_ResetConsole; 
  Rp->CallBack = win32CallBack;
  Rp->R_Quiet = FALSE;
  Rp->R_Interactive = TRUE;
  Rp->RestoreAction = SA_RESTORE;
  Rp->SaveAction = SA_SAVEASK;
  /* hocus-pocus for R-win32 - just don't ask why*/
  R_SetParams(Rp);
  R_SizeFromEnv(Rp);
  R_SetParams(Rp);
  setup_term_ui();
#endif

#ifdef RIF_HAS_RSIGHAND
  R_SignalHandlers = 0;
#endif  
  /* int status = Rf_initEmbeddedR(n_args, options);*/
  status = Rf_initialize_R(n_args, options);
  if (status < 0) {
    PyErr_SetString(PyExc_RuntimeError, "Error while initializing R.");
    return NULL;
  }

#if ! (defined(Win32) | defined(Win64))
  R_Interactive = TRUE;
#endif

#ifdef RIF_HAS_RSIGHAND
  R_SignalHandlers = 0;
#endif  

#ifdef R_INTERFACE_PTRS
  ptr_R_CleanUp = EmbeddedR_CleanUp;
  /* Redirect R console output */
  ptr_R_ShowMessage = EmbeddedR_ShowMessage;
  ptr_R_WriteConsole = NULL; /* Force use of WriteConsoleEx */
  ptr_R_WriteConsoleEx = EmbeddedR_WriteConsoleEx;
  ptr_R_FlushConsole = EmbeddedR_FlushConsole;
  ptr_R_ResetConsole = EmbeddedR_ResetConsole;
  R_Outputfile = NULL;
  R_Consolefile = NULL;
  /* Redirect R console input */
  ptr_R_ReadConsole = EmbeddedR_ReadConsole;
  ptr_R_ChooseFile = EmbeddedR_ChooseFile;
  ptr_R_ShowFiles = EmbeddedR_ShowFiles;
#endif

#ifdef CSTACK_DEFNS
  /* Taken from JRI:
   * disable stack checking, because threads will thow it off */
  R_CStackLimit = (uintptr_t) -1;
  /* --- */
#endif

  setup_Rmainloop();

  embeddedR_status = RPY_R_INITIALIZED;
  if (rpy2_setinitialized()) {
    printf("R is already initialized !");
  }

  SexpObject *sexpobj_ptr = Rpy_PreserveObject(R_GlobalEnv);
  Rpy_ReleaseObject(globalEnv->sObj->sexp);
  globalEnv->sObj = sexpobj_ptr;

  sexpobj_ptr = Rpy_PreserveObject(R_BaseNamespace);
  Rpy_ReleaseObject(baseNameSpaceEnv->sObj->sexp);
  baseNameSpaceEnv->sObj = sexpobj_ptr;
  
  sexpobj_ptr = Rpy_PreserveObject(R_EmptyEnv);
  Rpy_ReleaseObject(emptyEnv->sObj->sexp);
  emptyEnv->sObj = sexpobj_ptr;

  sexpobj_ptr = Rpy_PreserveObject(R_MissingArg);
  Rpy_ReleaseObject(((PySexpObject *)MissingArg_Type_New(0))->sObj->sexp);
  ((PySexpObject *)MissingArg_Type_New(0))->sObj = sexpobj_ptr;
    
  sexpobj_ptr = Rpy_PreserveObject(R_NilValue);
  Rpy_ReleaseObject(((PySexpObject *)RNULL_Type_New(0))->sObj->sexp);
  ((PySexpObject *)RNULL_Type_New(0))->sObj = sexpobj_ptr;

  sexpobj_ptr = Rpy_PreserveObject(R_UnboundValue);
  Rpy_ReleaseObject(((PySexpObject *)UnboundValue_Type_New(0))->sObj->sexp);
  ((PySexpObject *)UnboundValue_Type_New(0))->sObj = sexpobj_ptr;

  sexpobj_ptr = Rpy_PreserveObject(R_NilValue);
  Rpy_ReleaseObject(rpy_R_NilValue->sObj->sexp);
  rpy_R_NilValue->sObj = sexpobj_ptr;

  errMessage_SEXP = findVar(install("geterrmessage"), 
                            R_BaseNamespace);
  PyObject *res = PyLong_FromLong(status);

  /* type tag for Python external methods */
  SEXP type_tag;
  PROTECT(type_tag = allocVector(STRSXP, 1));
  SET_STRING_ELT(type_tag, 0, mkChar("Python"));

  //R_PreserveObject(type_tag);
  sexpobj_ptr = Rpy_PreserveObject(type_tag);
  UNPROTECT(1);
  Rpy_ReleaseObject(R_PyObject_type_tag->sObj->sexp);
  R_PyObject_type_tag->sObj = sexpobj_ptr;

  /* register the symbols */
  RegisterExternalSymbols();

  /*FIXME: setting readline variables so R's oddly static declarations
   become harmless*/
#ifdef HAS_READLINE
  char *rl_completer, *rl_basic;
  rl_completer = strndup(rl_completer_word_break_characters, 200);
  rl_completer_word_break_characters = rl_completer;

  rl_basic = strndup(rl_basic_word_break_characters, 200);
  rl_basic_word_break_characters = rl_basic;
#endif
  /* --- */

#ifdef RPY_VERBOSE
  printf("R initialized - status: %i\n", status);
#endif

  int register_endr = Py_AtExit( end_r );
  if (register_endr != 0) {
    register_endr = PyErr_WarnEx(PyExc_RuntimeWarning, 
				 "'rpy2.rinterface.endr' could not be "
				 "registered as a cleanup function "
				 "(limit exceed).",
				 1);
    /*FIXME: what if -1 returned ? calling end_r will leave the process unable
     to try to initialize R anyway. */
  }

  /* FIXME: Attempt at using an R container distinct from R's PreciousList */
  /* (currently replaced by a Python dict and R's PreciousList) */
  if (preservehash == Py_True) {
    PROTECT(
	    RPY_R_PreciousEnv = rpy2_newenv(Rf_ScalarLogical(TRUE),
					    R_GlobalEnv, 
					    Rf_ScalarInteger(29))
	    );
    R_PreserveObject(RPY_R_PreciousEnv);
    UNPROTECT(1);
  }

  return res;
}

static PyObject* EmbeddedR_end(PyObject *self, Py_ssize_t fatal)
{
  /* FIXME: Have a reference count for R objects known to Python.
   * ending R will not be possible until all such objects are already
   * deallocated in Python ?
   *other possibility would be to have a fallback for "unreachable" objects ? 
   */

  end_r();

  Rf_endEmbeddedR((int)fatal);

  embeddedR_status = embeddedR_status & (! RPY_R_INITIALIZED);

  SexpObject *sexpobj_ptr = Rpy_PreserveObject(R_EmptyEnv);
  Rpy_ReleaseObject(globalEnv->sObj->sexp);
  globalEnv->sObj = sexpobj_ptr;

  sexpobj_ptr = Rpy_PreserveObject(R_EmptyEnv);
  Rpy_ReleaseObject(baseNameSpaceEnv->sObj->sexp);
  baseNameSpaceEnv->sObj = sexpobj_ptr;

  sexpobj_ptr = Rpy_PreserveObject(R_EmptyEnv);
  Rpy_ReleaseObject(emptyEnv->sObj->sexp);
  emptyEnv->sObj = sexpobj_ptr;

  errMessage_SEXP = R_NilValue; 

  /* FIXME: Is it possible to reinitialize R later ?
   * Py_XDECREF(embeddedR_isInitialized);
   * embeddedR_isInitialized = Py_False;
   *Py_INCREF(embeddedR_isInitialized); 
   */

  Py_RETURN_NONE;
}
PyDoc_STRVAR(EmbeddedR_end_doc,
             "endr(int) -> None\n"
             "\n"
             "Terminate an embedded R by calling the function Rf_endEmbeddedR(int fatal) in the R's C-API."
	     " 0 (zero) seems to be be a commonly used integer when calling Rf_endEmbeddedR."
             " Note that a terminated R cannot be restarted from the same process.");



static PyObject* EmbeddedR_setinteractive(PyObject *self, PyObject *status)
{
  if (! PyBool_Check(status)) {
    PyErr_SetString(PyExc_ValueError, "The status must be a boolean");
    return NULL;
  }
  int rtruefalse;
  if (PyObject_IsTrue(status)) {
    rtruefalse = TRUE;
  } else {
    rtruefalse = FALSE;
  }
#if defined(Win32) || defined(Win64)
  Rp->R_Interactive = rtruefalse;
#else
  R_Interactive = rtruefalse;
#endif
  Py_RETURN_NONE;
}
PyDoc_STRVAR(EmbeddedR_setinteractive_doc,
             "set_interactive(status)\n\
             \n\
             Set the interactivity status for R.\n\
             (This function exists for experimentation purposes,\n\
             and could lead to an unpredictable outcome.)");




/* Create a Python exception from an R error */
static void
  EmbeddedR_exception_from_errmessage(PyObject *PythonException_Type)
{
  SEXP expr, res;
  /* PROTECT(errMessage_SEXP) */
  PROTECT(expr = allocVector(LANGSXP, 1));
  SETCAR(expr, errMessage_SEXP);
  PROTECT(res = Rf_eval(expr, R_GlobalEnv));
  const char *message = CHARACTER_VALUE(res);
  //RPyExc_RuntimeError
  //PyErr_SetString(RPyExc_RuntimeError, message);
  PyErr_SetString(PythonException_Type, message);
  UNPROTECT(2);
}


PyDoc_STRVAR(EmbeddedR_parse_doc,
             "parse(string)\n\
             \n\
             Parse a string as R code.\n");

static PySexpObject*
EmbeddedR_parse(PyObject *self, PyObject *pystring)
{

  SEXP cmdSexp, cmdexpr;
  PySexpObject *cmdpy;
  ParseStatus status;

  if (! (rpy_has_status(RPY_R_INITIALIZED))) {
    PyErr_Format(PyExc_RuntimeError, 
                 "R must be initialized before any call to R functions is possible.");
    return NULL;
  }

  PyObject *pybytes;
  char *string;
  if (! PyUnicode_Check(pystring)) {
    PyErr_Format(PyExc_ValueError, "The object to parse must be a unicode string");
    return NULL;
  }
  pybytes = PyUnicode_AsUTF8String(pystring);
  string = PyBytes_AsString(pybytes);

  embeddedR_setlock();

  PROTECT(cmdSexp = allocVector(STRSXP, 1));
  SET_STRING_ELT(cmdSexp, 0, mkChar(string));
  Py_DECREF(pybytes);
  cmdexpr = PROTECT(R_ParseVector(cmdSexp, -1, &status, R_NilValue));
  switch(status) {
  case PARSE_OK:
    cmdpy = newPySexpObject(cmdexpr);
    break;
  case PARSE_INCOMPLETE:
    PyErr_SetString(RPyExc_ParsingIncompleteError, "Incomplete R code statement.");
    cmdpy = NULL;
    break;
  default:
    EmbeddedR_exception_from_errmessage(RPyExc_ParsingError);
    cmdpy = NULL;
    break;
  }
  UNPROTECT(2);
  embeddedR_freelock();
  return cmdpy;  
}

/*
 * Access to R objects through Python objects
 */


/*
 * Closure-type Sexp.
 */


/* Evaluate a SEXP. It must be constructed by hand. It raises a Python
   exception if an error ocurred during the evaluation */
SEXP do_eval_expr(SEXP expr_R, SEXP env_R) {

  SEXP res_R = R_NilValue;
  int errorOccurred = 0;

  /* FIXME: if env_R is null, use R_BaseEnv
   * shouldn't it be R_GlobalContext (but then it throws a NULL error) ? */
  if (isNull(env_R)) {
    /* env_R = R_BaseEnv; */
    env_R = R_GlobalEnv;
    /* env_R = R_GlobalContext; */
  }

  /* Py_BEGIN_ALLOW_THREADS */

#ifdef _WIN32  
  last_sighandler = PyOS_setsig(SIGBREAK, interrupt_R);
#else
  last_sighandler = PyOS_setsig(SIGINT, interrupt_R);
#endif

  python_sighandler = last_sighandler;

  /* FIXME: evaluate expression in the given environment */
  interrupted = 0;
  res_R = R_tryEval(expr_R, env_R, &errorOccurred);

  /* Py_END_ALLOW_THREADS */
#ifdef _WIN32
  PyOS_setsig(SIGBREAK, python_sighandler);   
#else 
  PyOS_setsig(SIGINT, python_sighandler);
#endif
  if (errorOccurred) {
    res_R = R_NilValue;
    if (interrupted) {
      printf("Keyboard interrupt.\n");
      PyErr_SetNone(PyExc_KeyboardInterrupt);
      /* FIXME: handling of interruptions */
    } else {
      EmbeddedR_exception_from_errmessage(RPyExc_RuntimeError);
    }
  }
  return res_R;
}



static PyTypeObject EnvironmentSexp_Type;

/* This is the method to call when invoking a 'Sexp' */
static PyObject *
Sexp_rcall(PyObject *self, PyObject *args)
{
  if (! (rpy_has_status(RPY_R_INITIALIZED))) {
    PyErr_Format(PyExc_RuntimeError, 
                 "R must be initialized before any call to R functions is possible.");
    return NULL;
  }

  PyObject *params, *env;

  if (! PyArg_ParseTuple(args, "OO",
                         &params, &env)) {
    return NULL;
  }

  if (! PyTuple_Check(params)) {
    PyErr_Format(PyExc_ValueError, "The first parameter must be a tuple.");
    return NULL;
  }
  if (! PyObject_IsInstance(env,
                            (PyObject*)&EnvironmentSexp_Type)) {
    PyErr_Format(PyExc_ValueError, 
                 "The second parameter must be an EnvironmentSexp_Type.");
    return NULL;
  }

  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return NULL;
  }
  embeddedR_setlock();
    
  SEXP call_R, c_R, res_R;
  int nparams;
  SEXP tmp_R, fun_R;
  int protect_count = 0;
  
  if (! PySequence_Check(args)) {
    PyErr_Format(PyExc_ValueError, 
                 "The one argument to the function must implement the sequence protocol.");
    embeddedR_freelock();
    return NULL;
  }
  nparams = PySequence_Length(params);
  
  /* A SEXP with the function to call and the arguments and keywords. */
  PROTECT(c_R = call_R = allocList(nparams+1));
  protect_count++;
  SET_TYPEOF(call_R, LANGSXP);
  fun_R = RPY_SEXP((PySexpObject *)self);
  if (! fun_R) {
    PyErr_Format(PyExc_ValueError, "Underlying R function is a NULL SEXP.");
    goto fail;
  }
  SETCAR(c_R, fun_R);
  c_R = CDR(c_R);

  int arg_i;
  int on_the_fly; /* boolean flag to tell whether a given parameter is
                   * converted on the fly */
  PyObject *tmp_obj;  /* temp object to iterate through the args tuple*/

  /* named args */
  PyObject *argValue, *argName;
  PyObject *pybytes;
  const char *argNameString;
  unsigned int addArgName;
  Py_ssize_t item_length;
  /* Loop through the elements in the sequence "args"
   * and build the R call.
   * Each element in the sequence is expected to be a tuple
   * of length 2 (name, value).
   */
  for (arg_i=0; arg_i<nparams; arg_i++) {
    /* printf("item: %i\n", arg_i); */
    tmp_obj = PySequence_GetItem(params, arg_i);
    /* PySequence_GetItem() returns a new reference 
    * so tmp_obj must be DECREFed when no longer required */
    if (! tmp_obj) {
      PyErr_Format(PyExc_ValueError, "No item %i !?", arg_i);
      goto fail;
    }
    if (! PyTuple_Check(tmp_obj)) {
      PyErr_Format(PyExc_ValueError, 
                   "Item %i in the sequence is not a tuple.",
                   arg_i);
      Py_DECREF(tmp_obj);
      goto fail;
    }
    item_length = PyTuple_GET_SIZE(tmp_obj);
    if (item_length != 2) {
      PyErr_Format(PyExc_ValueError, "Item %i in the sequence passed as an argument"
                   "should have two elements.", 
                   arg_i);
      Py_DECREF(tmp_obj);
      goto fail;
    }
    /* First check the name for the parameter */
    /* (stolen reference, no need to look after it)*/
    argName = PyTuple_GET_ITEM(tmp_obj, 0);
    if (argName == Py_None) {
      addArgName = 0;
    } else if (PyUnicode_Check(argName) && (PyUnicode_GET_LENGTH(argName) > 0)) {
      addArgName = 1;
    } else {
      PyErr_SetString(PyExc_TypeError, "All keywords must be non-empty strings (or None).");
      Py_DECREF(tmp_obj);
      goto fail;
    }
    /* Then take care of the value associated with that name. */
    /* (stolen reference, no need to look after it)*/
    argValue = PyTuple_GET_ITEM(tmp_obj, 1);
    on_the_fly = 0;
    if (PyObject_TypeCheck(argValue, &Sexp_Type)) {
      tmp_R = RPY_SEXP((PySexpObject *)argValue);
      Py_DECREF(tmp_obj);
      /* tmp_R = Rf_duplicate(tmp_R); */
    } else {
      on_the_fly = 1;
      RPY_PYSCALAR_RVECTOR(argValue, tmp_R);
      if (tmp_R == NULL) {
        PyErr_Format(PyExc_ValueError, 
                     "All parameters must be of type Sexp_Type,"
                     "or Python int/long, float, bool, or None"
                     );
        Py_DECREF(tmp_obj);
        goto fail;
      }
    }
    
    if (! tmp_R) {
      PyErr_Format(PyExc_ValueError, "NULL SEXP.");
      Py_DECREF(tmp_obj);
      if (on_the_fly) {
        UNPROTECT(1);
        protect_count--;
      }
      goto fail;
    }
    /* Put the value into the R call being built
    * (adding the name of the argument if relevant)
    */
    SETCAR(c_R, tmp_R);
    if (addArgName) {
      pybytes = PyUnicode_AsUTF8String(argName);
      argNameString = Rf_mkCharCE(PyBytes_AsString(pybytes), CE_UTF8);
      SET_TAG(c_R, installChar(argNameString));

    Py_DECREF(pybytes);
    }
    c_R = CDR(c_R);
    /* if on-the-fly conversion, UNPROTECT the newly created
     * tmp_R in order to avoid overflowing the protection stack. */
    if (on_the_fly) {
      UNPROTECT(1);
      protect_count--;
    }
  }

  /* Py_BEGIN_ALLOW_THREADS */
  /* FIXME: R_GlobalContext ? */
  PROTECT(res_R = do_eval_expr(call_R, RPY_SEXP((PySexpObject *)env)));
  protect_count += 1;

  if (PyErr_Occurred()) {
    /* Python exception set during the call to do_eval_expr() */
    if (res_R == R_NilValue) {
      UNPROTECT(protect_count);
      embeddedR_freelock();
      return NULL;
    } else {
      printf("Warning: Exception while result not R_NilValue.\n");
    }
  }

  /* Unexplained hidding of the function R_PrintWarnings()
   * in the codebase (and inquiries about alternative options
   * on the R-dev list completely ignored).
   */
  /* FIXME: standardize R outputs */
  /* extern void Rf_PrintWarnings(void); */
  /* Rf_PrintWarnings(); /\* show any warning messages *\/ */

  PyObject *res = (PyObject *)newPySexpObject(res_R);
  UNPROTECT(protect_count);
  embeddedR_freelock();
  return res;
  
 fail:
  UNPROTECT(protect_count);
  embeddedR_freelock();
  return NULL;

}

PyDoc_STRVAR(SexpClosure_rcall_doc,
             "S.rcall(args, env) -> Sexp\n\n"
             "Return the result of evaluating the underlying R function"
             " as an instance of type rpy2.rinterface.Sexp,"
             " args being a sequence of two-elements items"
             " and env a instance of type rpy2.rinterface.SexpEnvironment.");


/* This is merely a wrapper around Sexp_rcall,
 * putting named and unnamed arguments into a tuple of name, value pairs.
 */
static PyObject *
Sexp_call(PyObject *self, PyObject *args, PyObject *kwds)
{

  Py_ssize_t n_unnamedparams, n_namedparams, n_params, p_i, ppos;
  PyObject *tmp_pair, *tmp_key, *tmp_value, *params, *new_args, *res;
  n_unnamedparams = PySequence_Length(args);
  /* test present in Objects/funcobject.c in the Python source 
   * Missing keywords do not translate to an empty dict.
   */
  if (kwds != NULL && PyDict_Check(kwds)) {
    n_namedparams = PyDict_Size(kwds);
  } else {
    n_namedparams = 0;
  }

  n_params = n_unnamedparams + n_namedparams;
  /* Tuple to hold (name, value) pairs for Sexp_rcall().
   * This must be DECREFed when exiting.
   */

  params = PyTuple_New(n_params);

  /* Populate with unnamed parameters first */
  for (p_i = 0; p_i < n_unnamedparams; p_i++) {

    tmp_pair = PyTuple_New(2);

    /* key/name is None */
    /* PyTuple_SET_ITEM() "steals" a reference, so INCREF necessary */
    Py_INCREF(Py_None);
    PyTuple_SET_ITEM(tmp_pair, 0, Py_None);

    /* value */
    tmp_value = PyTuple_GET_ITEM(args, p_i);
    Py_INCREF(tmp_value);
    PyTuple_SET_ITEM(tmp_pair, 1, tmp_value);

    PyTuple_SET_ITEM(params, p_i,
                     tmp_pair);
    /* PyTuple_SET_ITEM() "steals" a reference, 
       so no DECREF necessary */
  }

  if (n_namedparams > 0) {
    ppos = 0;
    p_i = 0;
    while(PyDict_Next(kwds, &ppos, &tmp_key, &tmp_value)) {

      tmp_pair = PyTuple_New(2);

      /* PyTuple_SET_ITEM() "steals" a reference, so no DECREF necessary */
      Py_INCREF(tmp_key);
      PyTuple_SET_ITEM(tmp_pair, 0, tmp_key);

      Py_INCREF(tmp_value);
      PyTuple_SET_ITEM(tmp_pair, 1, tmp_value);

      PyTuple_SET_ITEM(params, p_i + n_unnamedparams,
                       tmp_pair);
      p_i++;
      /* PyTuple_SET_ITEM() "steals" a reference, so no DECREF necessary */
    }
  }

  /* Build a tuple with the parameters for Sexp_rcall():
     - params built above
     - an R environment
     */
  new_args = PyTuple_New(2);
  PyTuple_SET_ITEM(new_args, 0, params);
  /* reference to params stolen, no need to change refcount for params */
  Py_INCREF(globalEnv);
  PyTuple_SET_ITEM(new_args, 1, (PyObject *)globalEnv);

  res = Sexp_rcall(self, new_args);
  Py_DECREF(new_args);
  return res;
}


static PySexpObject*
SexpClosure_env_get(PyObject *self)
{
  SEXP closureEnv, sexp;
  sexp = RPY_SEXP((PySexpObject*)self);
  if (! sexp) {
      PyErr_Format(PyExc_ValueError, "NULL SEXP.");
      return NULL;
  }
  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return NULL;
  }
  embeddedR_setlock();
  PROTECT(closureEnv = CLOENV(sexp));
  embeddedR_freelock();
  PySexpObject *res = newPySexpObject(closureEnv);
  UNPROTECT(1);
  return res;
}
PyDoc_STRVAR(SexpClosure_env_doc,
             "\n\
Environment the object is defined in.\n\
This corresponds to the C-level function CLOENV(SEXP).\n\
\n\
:rtype: :class:`rpy2.rinterface.SexpEnvironment`\n");

static PyMethodDef ClosureSexp_methods[] = {
  {"rcall", (PyCFunction)Sexp_rcall, METH_VARARGS,
   SexpClosure_rcall_doc},
  {NULL, NULL}          /* sentinel */
};


static PyGetSetDef ClosureSexp_getsets[] = {
  {"closureenv",
   (getter)SexpClosure_env_get,
   (setter)0,
   SexpClosure_env_doc},
  {NULL, NULL, NULL, NULL}
};

PyDoc_STRVAR(ClosureSexp_Type_doc,
"A R object that is a closure, that is a function. \
In R a function is defined within an enclosing \
environment, thus the name closure. \
In Python, 'nested scopes' could be the closest similar thing.\
");

static int
ClosureSexp_init(PyObject *self, PyObject *args, PyObject *kwds);


static PyTypeObject ClosureSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.SexpClosure",       /*tp_name*/
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
        Sexp_call,              /*tp_call*/
        0,                      /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,     /*tp_flags*/
        ClosureSexp_Type_doc,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        ClosureSexp_methods,           /*tp_methods*/
        0,                      /*tp_members*/
        ClosureSexp_getsets,    /*tp_getset*/
        &Sexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)ClosureSexp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        0,                      /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};


static int
ClosureSexp_init(PyObject *self, PyObject *args, PyObject *kwds)
{
  PyObject *object;
  PyObject *copy;
  static char *kwlist[] = {"sexpclos", "copy", NULL};
  /* FIXME: handle the copy argument */
  if (! PyArg_ParseTupleAndKeywords(args, kwds, "O|O!", 
                                    kwlist,
                                    &object,
                                    &PyBool_Type, &copy)) {
    return -1;
  }
  if (PyObject_IsInstance(object, 
                          (PyObject*)&ClosureSexp_Type)) {
    /* call parent's constructor */
    if (Sexp_init(self, args, NULL) == -1) {
      PyErr_Format(PyExc_RuntimeError, "Error initializing instance.");
      return -1;
    }
  } else {
    PyErr_Format(PyExc_ValueError, "Cannot instantiate from this type.");
    return -1;
  }
  return 0;
}



/* --- */
static PyObject*
EnvironmentSexp_findVar(PyObject *self, PyObject *args, PyObject *kwds)
{
  char *name;
  SEXP res_R = NULL;
  PySexpObject *res = NULL;
  PyObject *wantFun = Py_False;

  static char *kwlist[] = {"name", "wantfun", NULL};
 
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|O!",
                                   kwlist,
                                   &name, 
                                   &PyBool_Type, &wantFun)) { 
    return NULL; 
  }

  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return NULL;
  }
  embeddedR_setlock();

  const SEXP rho_R = RPY_SEXP((PySexpObject *)self);

  if (! rho_R) {
    PyErr_Format(PyExc_ValueError, "C-NULL SEXP.");
    embeddedR_freelock();
    return NULL;
  }
  if (!isEnvironment(rho_R)) {
    PyErr_Format(PyExc_ValueError, "Trying to apply to a non-environment (typeof is %i).", TYPEOF(rho_R));
    embeddedR_freelock();
    return NULL;
  }

  if (strlen(name) == 0) {
    PyErr_Format(PyExc_ValueError, "Invalid name.");
    embeddedR_freelock();
    return NULL;
  }

  if (rho_R == R_EmptyEnv) {
    PyErr_Format(PyExc_LookupError, "Fatal error: R_EmptyEnv.");
    return NULL;
  }

  if (PyObject_IsTrue(wantFun)) {
    res_R = rpy2_findfun(install(name), rho_R);
  } else {
    res_R = findVar(install(name), rho_R);
  }

  if (res_R != R_UnboundValue) {
    /* FIXME rpy_only */
    res = newPySexpObject(res_R);
  } else {
    PyErr_Format(PyExc_LookupError, "'%s' not found", name);
    res = NULL;
  }
  embeddedR_freelock();
  return (PyObject *)res;
}
PyDoc_STRVAR(EnvironmentSexp_findVar_doc,
             "Find a name/symbol in the environment, following the chain of enclosing\n"
             "environments until either the topmost environment is reached or the name\n"
             "is found, and returned the associated object. \n"
             "The optional parameter `wantfun` indicates whether functions should be\n"
             "returned or not.\n"
             ":rtype: instance of type of subtype :class:`rpy2.rinterface.Sexp`");

static PyObject*
EnvironmentSexp_frame(PyObject *self)
{
  if (! (rpy_has_status(RPY_R_INITIALIZED))) {
    PyErr_Format(PyExc_RuntimeError, 
                 "R must be initialized before environments can be accessed.");
    return NULL;
  }
  SEXP res_R = NULL;
  PySexpObject *res;

  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return NULL;
  }
  embeddedR_setlock();

  res_R = FRAME(RPY_SEXP((PySexpObject *)self));
  res = newPySexpObject(res_R);
  return (PyObject *)res;
}
PyDoc_STRVAR(EnvironmentSexp_frame_doc,
             "Return the frame the environment is in.");

static PyObject*
EnvironmentSexp_enclos(PyObject *self)
{
  if (! (rpy_has_status(RPY_R_INITIALIZED))) {
    PyErr_Format(PyExc_RuntimeError, 
                 "R must be initialized before environments can be accessed.");
    return NULL;
  }
  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return NULL;
  }
  embeddedR_setlock();

  SEXP res_R = NULL;
  PySexpObject *res;
  res_R = ENCLOS(RPY_SEXP((PySexpObject *)self));
  res = newPySexpObject(res_R);
  embeddedR_freelock();
  return (PyObject *)res;
}
PyDoc_STRVAR(EnvironmentSexp_enclos_doc,
             "Return the enclosure the environment is in.");

static PyObject* 
EnvironmentSexp_keys(PyObject *sexpEnvironment)
{
  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return NULL;
  }
  embeddedR_setlock();

  SEXP rho_R = RPY_SEXP((PySexpObject *)sexpEnvironment);

  if (! rho_R) {
    PyErr_Format(PyExc_ValueError, "The environment has NULL SEXP.");
    embeddedR_freelock();
    return NULL;
  }
  SEXP symbols, sexp_item;
  PROTECT(symbols = R_lsInternal(rho_R, TRUE));
  int l = LENGTH(symbols);
  PyObject *keys = PyTuple_New(l);
  PyObject *val;
  int i;
  for (i=0; i<l; i++) {
    sexp_item = STRING_ELT(symbols, i);
    const char *val_char = CHAR(sexp_item);
    val = PyUnicode_FromString(val_char);
    PyTuple_SET_ITEM(keys, i, val);
  }
  UNPROTECT(1);
  embeddedR_freelock();
  return keys;
}
PyDoc_STRVAR(EnvironmentSexp_keys_doc,
             "Return the symbols/names in the environment as a Python tuple.");


static PyMethodDef EnvironmentSexp_methods[] = {
  {"get", (PyCFunction)EnvironmentSexp_findVar, METH_VARARGS | METH_KEYWORDS,
  EnvironmentSexp_findVar_doc},
  {"frame", (PyCFunction)EnvironmentSexp_frame, METH_NOARGS,
  EnvironmentSexp_frame_doc},
  {"enclos", (PyCFunction)EnvironmentSexp_enclos, METH_NOARGS,
  EnvironmentSexp_enclos_doc},
  {"keys", (PyCFunction)EnvironmentSexp_keys, METH_NOARGS,
  EnvironmentSexp_keys_doc},
  {NULL, NULL}          /* sentinel */
};


static PySexpObject*
EnvironmentSexp_subscript(PyObject *self, PyObject *key)
{
  const char *name;
  SEXP res_R = NULL;

  if (!PyUnicode_Check(key)) {
    PyErr_Format(PyExc_ValueError, "Keys must be unicode string objects.");
    return NULL;
  }

  PyObject *pybytes = PyUnicode_AsUTF8String(key);
  if (pybytes == NULL) {
    return NULL;
  }
  name = PyBytes_AsString(pybytes);

  if (strlen(name) == 0) {
    PyErr_Format(PyExc_KeyError, "%s", name);
    Py_DECREF(pybytes);
    return NULL;
  }

  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    Py_DECREF(pybytes);
    return NULL;
  }
  embeddedR_setlock();

  SEXP rho_R = RPY_SEXP((PySexpObject *)self);
  if (! rho_R) {
    PyErr_Format(PyExc_ValueError, "C-NULL SEXP.");
    embeddedR_freelock();
    Py_DECREF(pybytes);
    return NULL;
  }

  /* use R's "get" */
  SEXP rsymb_internal = Rf_install(".Internal");
  SEXP rsymb_get = Rf_install("get");
  SEXP rlang_get = Rf_lang5(rsymb_get,
			    Rf_mkString(name), // x
  			    rho_R,
  			    Rf_mkString("any"),
   			    Rf_ScalarLogical(FALSE));
  SEXP rcall_get = Rf_lang2(rsymb_internal,
			    rlang_get);
  int errorOccurred = 0;
  res_R = R_tryEval(rcall_get, R_GlobalEnv, &errorOccurred);

  if (errorOccurred) {
    /* /\* 2 options here: no such key, or the somewhat entertaining */
    /*    "error on retrieve" (see issue #251) *\/ */
    SEXP rsymb_exists = Rf_install("exists");
    SEXP rlang_exists = Rf_lang5(rsymb_exists,
     				 Rf_mkString(name), // x
     				 rho_R,
     				 Rf_mkString("any"),
     				 Rf_ScalarLogical(FALSE));
    SEXP rcall_exists = Rf_lang2(rsymb_internal,
     				 rlang_exists);
    res_R = R_tryEvalSilent(rcall_exists, R_GlobalEnv, &errorOccurred);
    if (! asLogical(res_R)) {
      /* Error because of a missing key */
      PyErr_Format(PyExc_KeyError, "'%s' not found", name);
      Py_DECREF(pybytes);
      embeddedR_freelock();
      return NULL;
    } else {
      /* Retrieving the value associated with an existing key
         triggers an error in R. Don't ask. This is R. */
      res_R = R_NilValue;
      EmbeddedR_exception_from_errmessage(RPyExc_RuntimeError);
      Py_DECREF(pybytes);
      embeddedR_freelock();
      return NULL;
    }
  } else {
    /* No error */
    Py_DECREF(pybytes);
    embeddedR_freelock();
    return newPySexpObject(res_R);
  }
}

static int
EnvironmentSexp_ass_subscript(PyObject *self, PyObject *key, PyObject *value)
{
  const char *name;

  if (!PyUnicode_Check(key)) {
    PyErr_Format(PyExc_ValueError, "Keys must be unicode string objects.");
    return -1;
  }

  PyObject *pybytes = PyUnicode_AsUTF8String(key);
  name = PyBytes_AsString(pybytes);

  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    Py_DECREF(pybytes);
    return -1;
  }
  embeddedR_setlock();


  SEXP rho_R = RPY_SEXP((PySexpObject *)self);
  if (! rho_R) {
    PyErr_Format(PyExc_ValueError, "The environment has NULL SEXP.");
    embeddedR_freelock();
    Py_DECREF(pybytes);
    return -1;
  }
  

  SEXP sym;
  
  if (value == NULL) {
    /* deletion of the item 'key' */
    if (rho_R == R_BaseNamespace) {
      PyErr_Format(PyExc_ValueError, "Variables from the R base namespace cannot be removed.");
      embeddedR_freelock();
      Py_DECREF(pybytes);
      return -1;
    }
    if (rho_R == R_BaseEnv) {
      PyErr_Format(PyExc_ValueError, "Variables from the R base environment cannot be removed.");
      embeddedR_freelock();
      Py_DECREF(pybytes);
      return -1;
    }
    if (rho_R == R_EmptyEnv) {
      PyErr_Format(PyExc_ValueError, "Cannot remove variables from the empty environment.");
      embeddedR_freelock();
      Py_DECREF(pybytes);
      return -1;
    }
    if (R_EnvironmentIsLocked(rho_R)) {
      PyErr_Format(PyExc_ValueError, "Cannot remove bindings from a locked environment.");
      embeddedR_freelock();
      Py_DECREF(pybytes);
      return -1;
    }
    sym = Rf_install(name);
    SEXP res_rm;
    res_rm = findVarInFrame(rho_R, sym);
    
    if (res_rm == R_UnboundValue) {
      PyErr_Format(PyExc_KeyError, "'%s' not found", name);
      Py_DECREF(pybytes);
      embeddedR_freelock();
      return -1;
    }
    
    res_rm = rpy2_remove(Rf_mkString(name), 
			 rho_R, 
			 Rf_ScalarLogical(FALSE));
    if (! res_rm) {
      embeddedR_freelock();
      Py_DECREF(pybytes);
      PyErr_Format(PyExc_RuntimeError, "Could not remove variable from environment.");
      return -1;
    } else {
      embeddedR_freelock();
      Py_DECREF(pybytes);
      return 0;
    }
  }

  int is_PySexpObject = PyObject_TypeCheck(value, &Sexp_Type);
  if (! is_PySexpObject) {
    Py_DECREF(pybytes);
    embeddedR_freelock();
    PyErr_Format(PyExc_ValueError, 
                 "All parameters must be of type Sexp_Type.");
    return -1;
  }


  SEXP sexp_copy;
  SEXP sexp = RPY_SEXP((PySexpObject *)value);
  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "The value has NULL SEXP.");
    embeddedR_freelock();
    Py_DECREF(pybytes);
    return -1;
  }
    Py_DECREF(pybytes);
  sym = Rf_install(name);
  PROTECT(sexp_copy = Rf_duplicate(sexp));
  Rf_defineVar(sym, sexp_copy, rho_R);
  UNPROTECT(1);
  embeddedR_freelock();
  return 0;
}

static Py_ssize_t EnvironmentSexp_length(PyObject *self) 
{
  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return -1;
  }
  embeddedR_setlock();

  SEXP rho_R = RPY_SEXP((PySexpObject *)self);
  if (! rho_R) {
    PyErr_Format(PyExc_ValueError, "The environment has NULL SEXP.");
    embeddedR_freelock();
    return -1;
  }
  SEXP symbols;
  PROTECT(symbols = R_lsInternal(rho_R, TRUE));
  Py_ssize_t len = (Py_ssize_t)GET_LENGTH(symbols);
  UNPROTECT(1);
  embeddedR_freelock();
  return len;
}

static PyMappingMethods EnvironmentSexp_mappingMethods = {
  (lenfunc)EnvironmentSexp_length, /* mp_length */
  (binaryfunc)EnvironmentSexp_subscript, /* mp_subscript */
  (objobjargproc)EnvironmentSexp_ass_subscript  /* mp_ass_subscript */
};

static PyObject* 
EnvironmentSexp_iter(PyObject *sexpEnvironment)
{
  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return NULL;
  }
  embeddedR_setlock();

  SEXP rho_R = RPY_SEXP((PySexpObject *)sexpEnvironment);

  if (! rho_R) {
    PyErr_Format(PyExc_ValueError, "The environment has NULL SEXP.");
    embeddedR_freelock();
    return NULL;
  }
  SEXP symbols;
  PROTECT(symbols = R_lsInternal(rho_R, TRUE));
  PySexpObject *seq = newPySexpObject(symbols);
  Py_INCREF(seq);
  UNPROTECT(1);
 
  PyObject *it = PyObject_GetIter((PyObject *)seq);
  Py_DECREF(seq);
  embeddedR_freelock();
  return it;
}



PyDoc_STRVAR(EnvironmentSexp_Type_doc,
	     "R object that is an environment. "
	     "R environments can be seen as similar to Python "
	     "dictionnaries, with the following twists:\n"
	     "\n"
	     "- an environment can be a list of frames to sequentially\n"
	     "search into\n"
	     "\n"
	     "- the search can be sequentially propagated to the enclosing "
	     "environment(s) whenever the key is not found (in that respect "
	     "they can be seen as nested scopes).\n"
	     "\n"
	     "The subsetting operator \"[\" is made to match Python's "
	     "behavior, that is the enclosing environments are not "
	     "inspected upon absence of a given key. The method `get` should "
	     "be used instead.");


static int
EnvironmentSexp_init(PyObject *self, PyObject *args, PyObject *kwds);

static PyTypeObject EnvironmentSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.SexpEnvironment",   /*tp_name*/
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
        &EnvironmentSexp_mappingMethods,/*tp_as_mapping*/
        0,                      /*tp_hash*/
        0,              /*tp_call*/
        0,                      /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,     /*tp_flags*/
        EnvironmentSexp_Type_doc,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        EnvironmentSexp_iter,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        EnvironmentSexp_methods,           /*tp_methods*/
        0,                      /*tp_members*/
        0,                      /*tp_getset*/
        &Sexp_Type,             /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)EnvironmentSexp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        /* FIXME: add new method */
        0,                      /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};

static int
EnvironmentSexp_init(PyObject *self, PyObject *args, PyObject *kwds)
{
  PyObject *object;
  PyObject *copy = Py_False;
  static char *kwlist[] = {"sexpenv", "copy", NULL};
  /* FIXME: handle the copy argument */
  if (! PyArg_ParseTupleAndKeywords(args, kwds, "O|O!", 
                                    kwlist,
                                    &object,
                                    &PyBool_Type, &copy)) {
    return -1;
  }

  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return -1;
  }
  embeddedR_setlock();

  if (PyObject_IsInstance(object, 
                          (PyObject*)&EnvironmentSexp_Type)) {
    /* call parent's constructor */
    if (Sexp_init(self, args, NULL) == -1) {
      PyErr_Format(PyExc_RuntimeError, "Error initializing instance.");
      embeddedR_freelock();
      return -1;
    }
  } else {
    PyErr_Format(PyExc_ValueError, "Cannot instantiate from this type.");
    embeddedR_freelock();
    return -1;
  }
  embeddedR_freelock();
  return 0;
}


/* FIXME: write more doc */
PyDoc_STRVAR(S4Sexp_Type_doc,
"R object that is an 'S4 object'.\
Attributes can be accessed using the method 'do_slot'.\
");


static PyTypeObject S4Sexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.SexpS4",    /*tp_name*/
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
        S4Sexp_Type_doc,                      /*tp_doc*/
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
        0,                      /*tp_init*/
        0,                      /*tp_alloc*/
        /*FIXME: add new method */
        0,                     /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};


/* FIXME: write more doc */
PyDoc_STRVAR(SymbolSexp_Type_doc,
"R symbol");

static int
  SymbolSexp_init(PyObject *self, PyObject *args, PyObject *kwds);

static PyObject*
SymbolSexp_tp_str(PySexpObject *self)
{
  SEXP sexp = RPY_SEXP(self);
  /* if (! sexp) {
   *  PyErr_Format(PyExc_ValueError, "NULL SEXP.");
   *  return NULL;
   *}
   */
  const char* string = CHAR(PRINTNAME(sexp));
  return PyUnicode_FromString(string);
}

static PyTypeObject SymbolSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.SexpSymbol",  /*tp_name*/
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
        (reprfunc)SymbolSexp_tp_str,                      /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,     /*tp_flags*/
        SymbolSexp_Type_doc,                      /*tp_doc*/
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
        (initproc)SymbolSexp_init,                      /*tp_init*/
        0,                      /*tp_alloc*/
        /* FIXME: add new method */
        0,                      /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};

static int
SymbolSexp_init(PyObject *self, PyObject *args, PyObject *kwds)
{

  PyObject *pysymbol;
  PyObject *copy = Py_False;
  static char *kwlist[] = {"pysymbol", "copy", NULL};
  /* FIXME: handle the copy argument */
  if (! PyArg_ParseTupleAndKeywords(args, kwds, "O|O!", 
                                    kwlist,
                                    &pysymbol,
                                    &PyBool_Type, &copy)) {
    return -1;
  }

  if (rpy_has_status(RPY_R_BUSY)) {
    PyErr_Format(PyExc_RuntimeError, "Concurrent access to R is not allowed.");
    return -1;
  }
  embeddedR_setlock();
  
  SEXP rres = R_NilValue;
  /* Allow initialization from SYMSXP or from a Python string */
  int alreadySymbol = PyObject_IsInstance(pysymbol,
					  (PyObject*)&SymbolSexp_Type);
  if (alreadySymbol) {
    /* call parent's constructor */
    if (Sexp_init(self, args, NULL) == -1) {
      PyErr_Format(PyExc_RuntimeError, "Error initializing instance.");
      embeddedR_freelock();
      return -1;
    }
  }
  /* Only difference with Python < 3.1 is that PyString case is dropped. 
     Technically a macro would avoid code duplication.
    */
  else if (PyUnicode_Check(pysymbol)) {
    PyObject *utf8_str = PyUnicode_AsUTF8String(pysymbol);
    if (utf8_str == NULL) {
      //UNPROTECT(1);
      PyErr_Format(PyExc_ValueError,
		   "Error raised by codec for symbol");
      return -1;	
    }
    const char *string = PyBytes_AsString(utf8_str);
      rres = install(string);
      Py_XDECREF(utf8_str);
  }
  else {
    PyErr_Format(PyExc_ValueError, "Cannot instantiate from this type.");
    embeddedR_freelock();
    return -1;
  }
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


/* FIXME: write more doc */
PyDoc_STRVAR(LangSexp_Type_doc,
"Language object.");


static PyTypeObject LangSexp_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
        "rpy2.rinterface.SexpLang",  /*tp_name*/
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
        LangSexp_Type_doc,                      /*tp_doc*/
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
        0,                      /*tp_init*/
        0,                      /*tp_alloc*/
        /* FIXME: add new method */
        0,                      /*tp_new*/
        0,                      /*tp_free*/
        0                      /*tp_is_gc*/
};


/* --- Create a SEXP object --- 
 * Given an R SEXP object, it creates a 
 * PySexpObject that is an rpy2 Python representation
 * of an R object.
 *
 * In case of error, this returns NULL.
 */
static PySexpObject*
  newPySexpObject(const SEXP sexp)
{
  PySexpObject *object;
  SEXP sexp_ok, env_R;

  if (! sexp) {
    PyErr_Format(PyExc_ValueError, "NULL SEXP.");
    return NULL;
  }
  /* FIXME: let the possibility to manipulate un-evaluated promises ? */
  if (TYPEOF(sexp) == PROMSXP) {
    PROTECT(env_R = PRENV(sexp));
    if (env_R == R_NilValue) {
      env_R = R_BaseEnv;
    }
    PROTECT(sexp_ok = eval(sexp, env_R));
#ifdef RPY_DEBUG_PROMISE
    printf("  evaluating promise %p into %p.\n", sexp, sexp_ok);
#endif 
    UNPROTECT(2);
  } 
  else {
    sexp_ok = sexp;
  }

  switch (TYPEOF(sexp_ok)) {
  case NILSXP:
    object = (PySexpObject *)RNULL_Type_New(1);
    break;
  case SYMSXP:
    object  = (PySexpObject *)Sexp_new(&SymbolSexp_Type, Py_None, Py_None);
    break;
  case CLOSXP:
  case BUILTINSXP:
  case SPECIALSXP:
    object  = (PySexpObject *)Sexp_new(&ClosureSexp_Type, Py_None, Py_None);
    break;
    /*FIXME: BUILTINSXP and SPECIALSXP really like CLOSXP ? */
  case REALSXP:
    object = (PySexpObject *)Sexp_new(&FloatVectorSexp_Type, Py_None, Py_None);
    break;
  case INTSXP:
    object = (PySexpObject *)Sexp_new(&IntVectorSexp_Type, Py_None, Py_None);
    break;
  case LGLSXP:
    object = (PySexpObject *)Sexp_new(&BoolVectorSexp_Type, Py_None, Py_None);
    break;
  case STRSXP:
    object = (PySexpObject *)Sexp_new(&StrVectorSexp_Type, Py_None, Py_None);
    break;
  case VECSXP:
    object = (PySexpObject *)Sexp_new(&ListVectorSexp_Type, Py_None, Py_None);
    break;
  case CPLXSXP:
    object = (PySexpObject *)Sexp_new(&ComplexVectorSexp_Type, Py_None, Py_None);
    break;
  case LISTSXP:
  case LANGSXP:
  case EXPRSXP:
  case RAWSXP:
    object = (PySexpObject *)Sexp_new(&VectorSexp_Type, Py_None, Py_None);
    break;
  case ENVSXP:
    object = (PySexpObject *)Sexp_new(&EnvironmentSexp_Type, Py_None, Py_None);
    break;
  case S4SXP:
    object = (PySexpObject *)Sexp_new(&S4Sexp_Type, Py_None, Py_None);
    break;
  case EXTPTRSXP:
    object = (PySexpObject *)Sexp_new(&ExtPtrSexp_Type, Py_None, Py_None);
    break;
  default:
    object = (PySexpObject *)Sexp_new(&Sexp_Type, Py_None, Py_None);
    break;
  }

  if (!object) {
#ifdef RPY_DEBUG_PRESERVE
    printf("  PRESERVE -- Sexp_clear: R_ReleaseObject -- %p ", 
           sexp_ok);
    preserved_robjects -= 1;
    printf("-- %i\n", preserved_robjects);
#endif
    /* FIXME: Override possible error message from Rpy_ReleaseObject 
    (should an aggregated error message be made ? */
    PyErr_NoMemory();
    return NULL;
  }
  /* PyObject_Init(&object, &ClosureSexp_Type); */
  if (Rpy_ReplaceSexp(object, sexp_ok) == -1) {
    return NULL;
  }
  /* FIXME: Increment reference ? */
  /* Py_INCREF(object); */
  return object;
}


/* This function is only able to create a R-Python object 
   for an R vector-like 'rType', and from an 'object'
   that is a sequence. */
static SEXP
newSEXP(PyObject *object, int rType)
{
  SEXP sexp;
  SEXP str_R; /* used whenever there a string / unicode */
  PyObject *seq_object, *item, *item_tmp, *na, *pybytes; 

#ifdef RPY_VERBOSE
  printf("  new SEXP for Python:%p.\n", object);
#endif 

  seq_object = PySequence_Fast(object, 
                               "Cannot create R object from non-sequence Python object.");

  if (! seq_object) {
    return NULL;
  }
  const Py_ssize_t length = PySequence_Fast_GET_SIZE(seq_object);

  Py_ssize_t i;
  double *numeric_ptr;
  int   *integer_ptr;
  int    *logical_ptr;
  char *raw_ptr;
  switch(rType) {
  case REALSXP:
    PROTECT(sexp = NEW_NUMERIC(length));
    numeric_ptr = REAL(sexp);
    for (i = 0; i < length; ++i) {
      item = PySequence_Fast_GET_ITEM(seq_object, i);
      item_tmp = PyNumber_Float(item);
      if (item_tmp && (item != NAReal_New(0))) {
	  numeric_ptr[i] = PyFloat_AS_DOUBLE(item_tmp);
      } else {
        PyErr_Clear();
        numeric_ptr[i] = NA_REAL;
      }
      Py_XDECREF(item_tmp);
    }
    UNPROTECT(1);
    break;
  case INTSXP:
    PROTECT(sexp = NEW_INTEGER(length));
    integer_ptr = INTEGER(sexp);
    for (i = 0; i < length; ++i) {
      item = PySequence_Fast_GET_ITEM(seq_object, i);
      item_tmp = PyNumber_Long(item);
      if (item_tmp && (item != NAInteger_New(0))) {
	  long l = PyLong_AS_LONG(item_tmp);
	  integer_ptr[i] = RPY_RINT_FROM_LONG(l);
      } else {
        PyErr_Clear();
        integer_ptr[i] = NA_INTEGER;
      }
      Py_XDECREF(item_tmp);
    }
    UNPROTECT(1);
    break;
  case LGLSXP:
    PROTECT(sexp = NEW_LOGICAL(length));
    logical_ptr = LOGICAL(sexp);
    for (i = 0; i < length; ++i) {
      item = PySequence_Fast_GET_ITEM(seq_object, i);
      if (item == NALogical_New(0)) {
	logical_ptr[i] = NA_LOGICAL;
      } else {
	int q = PyObject_IsTrue(item);
	if (q != -1)
	  logical_ptr[i] = q;
	else {
	  PyErr_Clear();
	  logical_ptr[i] = NA_LOGICAL;
	}
      }
    }
    UNPROTECT(1);
    break;
  case RAWSXP:
    PROTECT(sexp = NEW_RAW(length));
    raw_ptr = (char *)RAW_POINTER(sexp);
    char *buffer;
    Py_ssize_t size_tmp;
    int ok;
    for (i = 0; i < length; ++i) {
      item = PySequence_Fast_GET_ITEM(seq_object, i);
      ok = PyBytes_AsStringAndSize(item, &buffer, &size_tmp);
      if (ok == -1) {
	PyErr_Clear();
	printf("Error while converting to Bytes element %zd.\n", i);
	continue;
      }
      if (size_tmp > 1) {
	/*FIXME: raise an error */
	printf("Invalid input for RAW. Truncating...\n");
      }
      raw_ptr[i] = buffer[0];
    }
    UNPROTECT(1);
    break;
  case STRSXP:
    PROTECT(sexp = NEW_CHARACTER(length));
    na = NACharacter_New(1);
    for (i = 0; i < length; ++i) {
      /* item is a borrowed reference */
      item = PySequence_Fast_GET_ITEM(seq_object, i);
      if (item == na) {
        str_R = NA_STRING;
      } else if (PyUnicode_Check(item)) {
	pybytes = PyUnicode_AsUTF8String(item);
	if (pybytes == NULL) {
	  sexp = NULL;
	  break;
	}
	const char *string = PyBytes_AsString(pybytes);
	if (string == NULL) {
	  Py_DECREF(pybytes);
	  sexp = NULL;
	  break;
	}
        str_R = mkCharCE(string, CE_UTF8);
	Py_DECREF(pybytes);
        if (!str_R) {
          PyErr_NoMemory();
          UNPROTECT(1);
          sexp = NULL;
          Py_DECREF(na);
          break;
        }
      }
      else {
        PyErr_Clear();
        str_R = NA_STRING;
      }
      SET_STRING_ELT(sexp, i, str_R);
    }
    UNPROTECT(1);
    Py_XDECREF(na);
    break;
  case VECSXP:
    PROTECT(sexp = NEW_LIST(length));
    SEXP tmp, tmp2;
    for (i = 0; i < length; ++i) {
      if((item = PySequence_Fast_GET_ITEM(seq_object, i))) {
        if (PyObject_TypeCheck(item, &Sexp_Type)) {
          SET_ELEMENT(sexp, i, RPY_SEXP((PySexpObject *)item));
        } else if (PyFloat_Check(item)) {
          tmp = allocVector(REALSXP, 1);
          REAL(tmp)[0] = PyFloat_AS_DOUBLE(item);
          SET_ELEMENT(sexp, i, tmp);
	} else if (PyLong_Check(item)) {
          tmp = allocVector(INTSXP, 1);
	  INTEGER_POINTER(tmp)[0] = (int)PyLong_AS_LONG(item);
          SET_ELEMENT(sexp, i, tmp);
        } else if (PyLong_Check(item)) {
          tmp = allocVector(INTSXP, 1);
          INTEGER_POINTER(tmp)[0] = RPY_RINT_FROM_LONG(PyLong_AsLong(item));
          if ((INTEGER_POINTER(tmp)[0] == -1) && PyErr_Occurred() ) {
            INTEGER_POINTER(tmp)[0] = NA_INTEGER;
            PyErr_Clear();
          }
          SET_ELEMENT(sexp, i, tmp);
        } else if (PyBool_Check(item)) {
          tmp = allocVector(LGLSXP, 1);
	  LOGICAL_POINTER(tmp)[0] = (int)PyLong_AS_LONG(item);
          SET_ELEMENT(sexp, i, tmp);
	} else if (PyUnicode_Check(item)) {
          PROTECT(tmp = NEW_CHARACTER(1));
	  pybytes = PyUnicode_AsUTF8String(item);
	  if (pybytes == NULL) {
	    sexp = NULL;
	    break;
	  }
	  tmp2 = mkCharCE(PyUnicode_AsUTF8(pybytes), CE_UTF8);
	  Py_DECREF(pybytes);
          if (!tmp2) {
            PyErr_NoMemory();
            sexp = NULL;
            break;
          }
          SET_STRING_ELT(tmp, 0, tmp2);
          SET_ELEMENT(sexp, i, tmp);
          UNPROTECT(1);
        } else {
          PyErr_Format(PyExc_ValueError, "All elements of the list must be of "
                       "type 'Sexp_Type' or of Python types float, int, bool, or str.");
          sexp = NULL;
          break;
        }
      }
    }
    UNPROTECT(1);
    break;
  case CPLXSXP:
    PROTECT(sexp = NEW_COMPLEX(length));
    for (i = 0; i < length; ++i) {
        item = PySequence_Fast_GET_ITEM(seq_object, i);
        if (PyComplex_Check(item)) {
          Py_complex cplx = PyComplex_AsCComplex(item);
          COMPLEX(sexp)[i].r = cplx.real;
          COMPLEX(sexp)[i].i = cplx.imag;
        }
        else {
          PyErr_Clear();
          COMPLEX(sexp)[i].r = NA_REAL;
          COMPLEX(sexp)[i].i = NA_REAL;
        }
    }
    UNPROTECT(1);
    break;
  default:
    PyErr_Format(PyExc_ValueError, "Cannot handle type %d", rType);
    sexp = NULL;
  }

  Py_DECREF(seq_object);

/*   if (sexp != NULL) { */
/*     //R_PreserveObject(sexp); */
/*     SexpObject *sexpobjet_ptr = Rpy_PreserveObject(sexp); */
/* #ifdef RPY_DEBUG_PRESERVE */
/*     preserved_robjects += 1; */
/*     printf("  PRESERVE -- R_PreserveObject -- %p -- %i\n",  */
/*            sexp, preserved_robjects); */
/* #endif  */
/*   } */

#ifdef RPY_VERBOSE
  printf("  new SEXP for Python:%p is %p.\n", object, sexp);
#endif
  return sexp;
}



/* --- Find a variable in an environment --- */


static PyObject*
EmbeddedR_sexpType(PyObject *self, PyObject *args)
{
  /* Return the C-defined name for R types */
  int sexp_i;

  if (! PyArg_ParseTuple(args, "i", &sexp_i)) {
    /* PyErr_Format(PyExc_LookupError, "Value should be an integer"); */
    return NULL;
  }

  const char *sexp_type = validSexpType[sexp_i];

  if ((sexp_i < 0) || (sexp_i >= RPY_MAX_VALIDSEXTYPE) || (! sexp_type)) {

    PyErr_Format(PyExc_LookupError, "'%i' is not a valid SEXP value.", sexp_i);
    return NULL;
  }
  /* FIXME: store python strings when initializing validSexpType instead */
  PyObject *res = PyUnicode_FromString(sexp_type);
  return res;

}


/* --- List of functions defined in the module --- */

static PyMethodDef EmbeddedR_methods[] = {
  {"get_initoptions",     (PyCFunction)EmbeddedR_getinitoptions,   
   METH_NOARGS,
   EmbeddedR_get_initoptions_doc},
  {"set_initoptions",     (PyCFunction)EmbeddedR_setinitoptions,   
   METH_O,
   EmbeddedR_set_initoptions_doc},
  {"initr",     (PyCFunction)EmbeddedR_init, METH_VARARGS | METH_KEYWORDS,
   EmbeddedR_init_doc},
  {"is_initialized", (PyCFunction)EmbeddedR_isInitialized, METH_NOARGS,
   EmbeddedR_isInitialized_doc},
  {"endr",      (PyCFunction)EmbeddedR_end,    METH_O,
   EmbeddedR_end_doc},
  {"set_interactive",   (PyCFunction)EmbeddedR_setinteractive,  METH_O,
   EmbeddedR_setinteractive_doc},
  {"set_writeconsole_regular",   (PyCFunction)EmbeddedR_setWriteConsoleRegular, 
   METH_VARARGS, EmbeddedR_setWriteConsoleRegular_doc},
  {"get_writeconsole_regular",   (PyCFunction)EmbeddedR_getWriteConsoleRegular,  
   METH_VARARGS, EmbeddedR_getWriteConsoleRegular_doc},
  {"set_writeconsole_warnerror",   (PyCFunction)EmbeddedR_setWriteConsoleWarnError, 
   METH_VARARGS, EmbeddedR_setWriteConsoleWarnError_doc},
  {"get_writeconsole_warnerror",   (PyCFunction)EmbeddedR_getWriteConsoleWarnError,  
   METH_VARARGS, EmbeddedR_getWriteConsoleWarnError_doc},
  {"set_readconsole",    (PyCFunction)EmbeddedR_setReadConsole,   METH_VARARGS,
   EmbeddedR_setReadConsole_doc},
  {"get_readconsole",    (PyCFunction)EmbeddedR_getReadConsole,   METH_VARARGS,
   EmbeddedR_getReadConsole_doc},
  {"set_flushconsole",   (PyCFunction)EmbeddedR_setFlushConsole,  METH_VARARGS,
   EmbeddedR_setFlushConsole_doc},
  {"get_flushconsole",   (PyCFunction)EmbeddedR_getFlushConsole,  METH_VARARGS,
   EmbeddedR_getFlushConsole_doc},
  {"set_resetconsole",   (PyCFunction)EmbeddedR_setResetConsole,  METH_VARARGS,
   EmbeddedR_setResetConsole_doc},
  {"get_resetconsole",   (PyCFunction)EmbeddedR_getResetConsole,  METH_VARARGS,
   EmbeddedR_getResetConsole_doc},
  {"set_showmessage",    (PyCFunction)EmbeddedR_setShowMessage,   METH_VARARGS,
   EmbeddedR_setShowMessage_doc},
  {"get_showmessage",    (PyCFunction)EmbeddedR_getShowMessage,   METH_VARARGS,
   EmbeddedR_getShowMessage_doc},
  {"set_choosefile",     (PyCFunction)EmbeddedR_setChooseFile,    METH_VARARGS,
   EmbeddedR_setChooseFile_doc},
  {"get_choosefile",     (PyCFunction)EmbeddedR_getChooseFile,    METH_VARARGS,
   EmbeddedR_getChooseFile_doc},
  {"set_showfiles",      (PyCFunction)EmbeddedR_setShowFiles,     METH_VARARGS,
   EmbeddedR_setShowFiles_doc},
  {"get_showfiles",      (PyCFunction)EmbeddedR_getShowFiles,     METH_VARARGS,
   EmbeddedR_getShowFiles_doc},
  {"set_cleanup",      (PyCFunction)EmbeddedR_setCleanUp,     METH_VARARGS,
   EmbeddedR_setCleanUp_doc},
  {"get_cleanup",      (PyCFunction)EmbeddedR_getCleanUp,     METH_VARARGS,
   EmbeddedR_getCleanUp_doc},
  {"parse",  (PyCFunction)EmbeddedR_parse,  METH_O,
   EmbeddedR_parse_doc},
  {"process_revents", (PyCFunction)EmbeddedR_ProcessEvents, METH_NOARGS,
   EmbeddedR_ProcessEvents_doc},
  {"str_typeint",       (PyCFunction)EmbeddedR_sexpType, METH_VARARGS,
   "Return the SEXP name tag (string) corresponding to an integer."},
  {"unserialize",       (PyCFunction)EmbeddedR_unserialize, METH_VARARGS,
   "unserialize(str, rtype)\n"
   "Unserialize an R object from its string representation."},
  {"protected_rids", (PyCFunction)Rpy_ProtectedIDs, METH_NOARGS,
   Rpy_ProtectedIDs_doc},
  {NULL,                NULL}           /* sentinel */
};




/* A. Belopolsky's callback */

/* R representation of a PyObject */

static SEXP
mkPyObject(PyObject* pyo)
{
  SEXP res;
  Py_INCREF(pyo);
  res = R_MakeExternalPtr(pyo, RPY_SEXP(R_PyObject_type_tag), R_NilValue);
  R_RegisterCFinalizer(res, (R_CFinalizer_t)R_PyObject_decref);
  return res;
}

#define R_PyObject_TYPE_CHECK(s)                                        \
  (TYPEOF(s) == EXTPTRSXP && R_ExternalPtrTag(s) == RPY_SEXP(R_PyObject_type_tag))

static SEXP
do_Python(SEXP args)
{
  args = CDR(args);
  SEXP sexp = CAR(args);
  SEXP res;
  int protect_count;

  if (!R_PyObject_TYPE_CHECK(sexp)) {
    error(".Python: The first argument must be an external pointer tagged as of Python type.");
    return R_NilValue;
  }
  PyObject *pyf = R_ExternalPtrAddr(sexp);

  /* Result for the evaluation of the Python function */
  PyObject *pyres;
  /* create argument list */
  PyObject *pyargs = PyList_New(0);
  /* named arguments */
  PyObject *pynargs = PyDict_New();
  const char *tag;
  int ok_setnamedarg;
  for (args = CDR(args); args != R_NilValue; args = CDR(args)) {
    sexp = CAR(args);
    if (isNull(TAG(args))) {
      /* unnamed argument */
      if (R_PyObject_TYPE_CHECK(sexp)) {
	PyList_Append(pyargs, (PyObject *)R_ExternalPtrAddr(sexp));
      }
      else {
	PyList_Append(pyargs, (PyObject *)newPySexpObject(sexp));
      }
    } else {
      tag = CHAR(PRINTNAME(TAG(args)));
      /* named argument */
      if (R_PyObject_TYPE_CHECK(sexp)) {
	ok_setnamedarg = PyDict_SetItemString(pynargs, tag, 
					      (PyObject *)R_ExternalPtrAddr(sexp));
      }
      else {
	ok_setnamedarg = PyDict_SetItemString(pynargs, tag, 
					      (PyObject *)newPySexpObject(sexp));
      }
      if (ok_setnamedarg == -1) {
	error("rpy2: Error while setting a named argument");
      }
    }
  }

  PyObject *pyargstup = PyList_AsTuple(pyargs);

  /* free the R lock as we are leaving the R side back to Python */
  embeddedR_freelock();
  pyres = PyObject_Call(pyf, pyargstup, pynargs);
  embeddedR_setlock();
  if (!pyres) {
    PyObject *exctype;
    PyObject *excvalue; 
    PyObject *exctraceback;
    PyObject *excstr;
    PyErr_Fetch(&exctype, &excvalue, &exctraceback);
    excstr = PyObject_Str(excvalue);
    if (excstr) {
      PyObject *pybytes = PyUnicode_AsLatin1String(excstr);
      error(PyBytes_AsString(pybytes));
      Py_DECREF(pybytes);
      Py_DECREF(excstr);
    } 
    else {
      error("rpy2: Python error.");
    }
    PyErr_Clear();
  }
  Py_DECREF(pyargs);
  Py_DECREF(pyargstup);
  if (PyObject_IsInstance((PyObject*)pyres, 
                          (PyObject*)&Sexp_Type)) {
    res = RPY_SEXP((PySexpObject*)pyres);
  }
  else {
    protect_count = 0;
    RPY_PYSCALAR_RVECTOR(pyres, res);
    if (res == NULL) {
      res = mkPyObject(pyres);
    }
    UNPROTECT(protect_count);
  }
  Py_DECREF(pyres);
  
  return res;
}

static void RegisterExternalSymbols() {
  R_ExternalMethodDef externalMethods[] = { 
    {".Python", (DL_FUNC)&do_Python, -1},
    {NULL, NULL, 0} 
  };
  R_registerRoutines(R_getEmbeddingDllInfo(), NULL, NULL, NULL, externalMethods);
}




/* --- Initialize the module ---*/
static char **validSexpType;


#define ADD_INT_CONSTANT(module, name) \
  PyModule_AddIntConstant(module, #name, name); \

#define ADD_SEXP_CONSTANT(module, name)         \
  PyModule_AddIntConstant(module, #name, name); \
  validSexpType[name] = #name ;                 \

#define PYASSERT_ZERO(code) \
  if ((code) != 0) {return ; } \

static struct PyModuleDef rinterfacemodule = {
   PyModuleDef_HEAD_INIT,
   "_rinterface",           /* name of module */
   module_doc,               /* module documentation, may be NULL */
   -1,                     /* size of per-interpreter state */
   EmbeddedR_methods       /* method table */
 };

/* GS: Necessary? */
/* LG: might be for win32/win64 (I can't remember)*/
#ifndef PyMODINIT_FUNC  /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif

PyMODINIT_FUNC
PyInit__rinterface(void)
{
  /* PyMODINIT_FUNC */
  /* RPY_RINTERFACE_INIT(void) */
  /* { */

  /* Finalize the type object including setting type of the new type
         * object; doing it here is required for portability to Windows 
         * without requiring C++. */
  if (PyType_Ready(&Sexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&SymbolSexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&ClosureSexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&VectorSexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&IntVectorSexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&FloatVectorSexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&StrVectorSexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&BoolVectorSexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&ByteVectorSexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&ComplexVectorSexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&ListVectorSexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&EnvironmentSexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&S4Sexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&LangSexp_Type) < 0) {
    return NULL;
  }
  if (PyType_Ready(&ExtPtrSexp_Type) < 0) {
    return NULL;
  }

  /* Required because NA types inherit from basic Python types */
  if (PyType_Ready(&PyBool_Type) < 0)  {
    return NULL;
  }

  if (PyType_Ready(&PyLong_Type) < 0) {
    return NULL;
  }

  /* NA types */
#if defined(Win32) || defined(Win64)
  NAInteger_Type.tp_base=&PyLong_Type;
#endif
  if (PyType_Ready(&NAInteger_Type) < 0) {
    return NULL;
  }
#if defined(Win32) || defined(Win64)
  NALogical_Type.tp_base=&PyLong_Type;
#endif
  if (PyType_Ready(&NALogical_Type) < 0) {
    return NULL;
  }
#if defined(Win32) || defined(Win64)
  NAReal_Type.tp_base=&PyFloat_Type;
#endif
  if (PyType_Ready(&NAReal_Type) < 0) {
    return NULL;
  }
#if defined(Win32) || defined(Win64)
  NAComplex_Type.tp_base=&PyComplex_Type;
#endif
  if (PyType_Ready(&NAComplex_Type) < 0) {
    return NULL;
  }
  
#if defined(Win32)
  NACharacter_Type.tp_base=&PyUnicode_Type;
#endif

  if (PyType_Ready(&NACharacter_Type) < 0) {
    return NULL;
  }

  PyObject *m, *d;
  static void *PyRinterface_API[PyRinterface_API_pointers];
  PyObject *c_api_object;

  m = PyModule_Create(&rinterfacemodule);
  if (m == NULL) {
    return NULL;
  }

  /* Create a Capsule containing the API pointer array's address */
  c_api_object = PyCapsule_New((void *)PyRinterface_API, 
			       PyRinterface_API_NAME, NULL);
  if (c_api_object == NULL) {
    return NULL;
  } else {
    PyModule_AddObject(m, "SEXPOBJ_C_API", c_api_object);
  }
  d = PyModule_GetDict(m);

  /* Add SXP types */
  validSexpType = calloc(RPY_MAX_VALIDSEXTYPE, sizeof(char *));
  if (! validSexpType) {
    PyErr_NoMemory();
    return NULL;
  }

  ADD_SEXP_CONSTANT(m, NILSXP);
  ADD_SEXP_CONSTANT(m, SYMSXP);
  ADD_SEXP_CONSTANT(m, LISTSXP);
  ADD_SEXP_CONSTANT(m, CLOSXP);
  ADD_SEXP_CONSTANT(m, ENVSXP);
  ADD_SEXP_CONSTANT(m, PROMSXP);
  ADD_SEXP_CONSTANT(m, LANGSXP);
  ADD_SEXP_CONSTANT(m, SPECIALSXP);
  ADD_SEXP_CONSTANT(m, BUILTINSXP);
  ADD_SEXP_CONSTANT(m, CHARSXP);
  ADD_SEXP_CONSTANT(m, STRSXP);
  ADD_SEXP_CONSTANT(m, LGLSXP);
  ADD_SEXP_CONSTANT(m, INTSXP);
  ADD_SEXP_CONSTANT(m, REALSXP);
  ADD_SEXP_CONSTANT(m, CPLXSXP);
  ADD_SEXP_CONSTANT(m, DOTSXP);
  ADD_SEXP_CONSTANT(m, ANYSXP);
  ADD_SEXP_CONSTANT(m, VECSXP);
  ADD_SEXP_CONSTANT(m, EXPRSXP);
  ADD_SEXP_CONSTANT(m, BCODESXP);
  ADD_SEXP_CONSTANT(m, EXTPTRSXP);
  ADD_SEXP_CONSTANT(m, RAWSXP);
  ADD_SEXP_CONSTANT(m, S4SXP);

  /* longuest integer for R indexes */
  ADD_INT_CONSTANT(m, R_LEN_T_MAX);

  /* "Logical" (boolean) values */
  ADD_INT_CONSTANT(m, TRUE);
  ADD_INT_CONSTANT(m, FALSE);

  /* R_ext/Arith.h */
  /* ADD_INT_CONSTANT(m, NA_LOGICAL); */
  /* ADD_INT_CONSTANT(m, NA_INTEGER); */


  RPY_R_VERSION_BUILD = PyTuple_New(4);
  if (PyTuple_SetItem(RPY_R_VERSION_BUILD, 0, 
		      PyUnicode_FromString(R_MAJOR)) < 0) 
    return NULL;
  if (PyTuple_SetItem(RPY_R_VERSION_BUILD, 1, 
		      PyUnicode_FromString(R_MINOR)) < 0) 
    return NULL;
  if (PyTuple_SetItem(RPY_R_VERSION_BUILD, 2, 
		      PyUnicode_FromString(R_STATUS)) < 0) 
    return NULL;

#if (R_VERSION < __RPY_RSVN_SWITCH_VERSION__)
  if (PyTuple_SetItem(RPY_R_VERSION_BUILD, 3, 
		      PyLong_FromLong(R_SVN_REVISION)) < 0) 
    return NULL;
#else
  if (PyTuple_SetItem(RPY_R_VERSION_BUILD, 3, 
		      PyLong_FromLong(R_SVN_REVISION)) < 0) 
    return NULL;
#endif

  initOptions = PyTuple_New(3);

  if (PyTuple_SetItem(initOptions, 0, PyBytes_FromString("rpy2")) < 0) 
    return NULL;
  if (PyTuple_SetItem(initOptions, 1, PyBytes_FromString("--quiet")) < 0)
    return NULL;
  /* if (PyTuple_SetItem(initOptions, 2, PyBytes_FromString("--vanilla")) < 0) */
  /*   return NULL; */
  if (PyTuple_SetItem(initOptions, 2, PyBytes_FromString("--no-save")) < 0)
    return NULL;

  /* Add an extra ref. It should remain impossible to delete it */
  Py_INCREF(initOptions);

  Rpy_R_Precious = PyDict_New();
  PyModule_AddObject(m, "_Rpy_R_Precious", Rpy_R_Precious);
  /* Add an extra ref. It should remain impossible to delete it */
  Py_INCREF(Rpy_R_Precious);

  PyModule_AddObject(m, "R_VERSION_BUILD", RPY_R_VERSION_BUILD);
  PyModule_AddObject(m, "initoptions", initOptions);
  PyModule_AddObject(m, "Sexp", (PyObject *)&Sexp_Type);
  PyModule_AddObject(m, "SexpSymbol", (PyObject *)&SymbolSexp_Type);
  PyModule_AddObject(m, "SexpClosure", (PyObject *)&ClosureSexp_Type);
  PyModule_AddObject(m, "SexpVector", (PyObject *)&VectorSexp_Type);
  PyModule_AddObject(m, "IntSexpVector", (PyObject *)&IntVectorSexp_Type);
  PyModule_AddObject(m, "FloatSexpVector", (PyObject *)&FloatVectorSexp_Type);
  PyModule_AddObject(m, "StrSexpVector", (PyObject *)&StrVectorSexp_Type);
  PyModule_AddObject(m, "BoolSexpVector", (PyObject *)&BoolVectorSexp_Type);
  PyModule_AddObject(m, "ByteSexpVector", (PyObject *)&ByteVectorSexp_Type);
  PyModule_AddObject(m, "ComplexSexpVector", (PyObject *)&ComplexVectorSexp_Type);
  PyModule_AddObject(m, "ListSexpVector", (PyObject *)&ListVectorSexp_Type);
  PyModule_AddObject(m, "SexpEnvironment", (PyObject *)&EnvironmentSexp_Type);
  PyModule_AddObject(m, "SexpS4", (PyObject *)&S4Sexp_Type);
  PyModule_AddObject(m, "SexpLang", (PyObject *)&LangSexp_Type);
  PyModule_AddObject(m, "SexpExtPtr", (PyObject *)&ExtPtrSexp_Type);

  /* NA types */
  PyModule_AddObject(m, "NAIntegerType", (PyObject *)&NAInteger_Type);
  PyModule_AddObject(m, "NA_Integer", NAInteger_New(1));
  PyModule_AddObject(m, "NALogicalType", (PyObject *)&NALogical_Type);
  PyModule_AddObject(m, "NA_Logical", NALogical_New(1));
  PyModule_AddObject(m, "NARealType", (PyObject *)&NAReal_Type);
  PyModule_AddObject(m, "NA_Real", NAReal_New(1));
  PyModule_AddObject(m, "NAComplexType", (PyObject *)&NAComplex_Type);
  PyModule_AddObject(m, "NA_Complex", NAComplex_New(1));
  PyModule_AddObject(m, "NACharacterType", (PyObject *)&NACharacter_Type);
  PyModule_AddObject(m, "NA_Character", NACharacter_New(1));

  /* Missing */
  if (PyType_Ready(&MissingArg_Type) < 0) {
    return NULL;
  }
  PyModule_AddObject(m, "MissingArgType", (PyObject *)&MissingArg_Type);
  PyModule_AddObject(m, "MissingArg", MissingArg_Type_New(1));

  /* Unbound */
  if (PyType_Ready(&UnboundValue_Type) < 0) {
    return NULL;
  }
  PyModule_AddObject(m, "UnboundValueType", (PyObject *)&UnboundValue_Type);
  PyModule_AddObject(m, "UnboundValue", UnboundValue_Type_New(1));

  /* NULL */
  if (PyType_Ready(&RNULL_Type) < 0) {
    return NULL;
  }
  PyModule_AddObject(m, "RNULLType", (PyObject *)&RNULL_Type);
  /*FIXME: shouldn't RNULLArg disappear ? */
  PyModule_AddObject(m, "RNULLArg", RNULL_Type_New(1));
  PyModule_AddObject(m, "NULL", RNULL_Type_New(1));


  if (RPyExc_RuntimeError == NULL) {
    RPyExc_RuntimeError = PyErr_NewException("rpy2.rinterface.RRuntimeError", 
                                             NULL, NULL);
    if (RPyExc_RuntimeError == NULL) {
      return NULL;
    }
  }

  Py_INCREF(RPyExc_RuntimeError);
  PyModule_AddObject(m, "RRuntimeError", RPyExc_RuntimeError);

  if (RPyExc_ParsingError == NULL) {
    RPyExc_ParsingError = \
      PyErr_NewExceptionWithDoc("rpy2.rinterface.RParsingError",
				"Error when parsing a string as R code.",
				NULL, NULL);
    if (RPyExc_ParsingError == NULL) {
      return NULL;
    }
  }
  Py_INCREF(RPyExc_ParsingError);
  PyModule_AddObject(m, "RParsingError", RPyExc_ParsingError);
  
  if (RPyExc_ParsingIncompleteError == NULL) {
    RPyExc_ParsingIncompleteError = \
      PyErr_NewExceptionWithDoc("rpy2.rinterface.RParsingIncompleteError",
				"Exception raised when a string parsed as"
				"R code seems like an incomplete code block.",
				RPyExc_ParsingError, NULL);
    if (RPyExc_ParsingIncompleteError == NULL) {
      return NULL;
    }
  }
  Py_INCREF(RPyExc_ParsingIncompleteError);
  PyModule_AddObject(m, "RParsingIncompleteError",
		     RPyExc_ParsingIncompleteError);

  emptyEnv = (PySexpObject*)Sexp_new(&EnvironmentSexp_Type,
                                     Py_None, Py_None);
  SexpObject *sexpobj_ptr = Rpy_PreserveObject(R_EmptyEnv);
  Rpy_ReleaseObject(emptyEnv->sObj->sexp);
  emptyEnv->sObj = sexpobj_ptr;
  if (PyDict_SetItemString(d, "emptyenv", 
                           (PyObject *)emptyEnv) < 0)
  {
    Py_DECREF(emptyEnv);
    return NULL;
  }
  Py_DECREF(emptyEnv);

  globalEnv = (PySexpObject *)Sexp_new(&EnvironmentSexp_Type, 
                                       Py_None, Py_None);
  sexpobj_ptr = Rpy_PreserveObject(R_EmptyEnv);
  Rpy_ReleaseObject(globalEnv->sObj->sexp);
  globalEnv->sObj = sexpobj_ptr;

  if (PyDict_SetItemString(d, "globalenv", (PyObject *)globalEnv) < 0)
  {
    Py_DECREF(globalEnv);
    return NULL;
  }
  Py_DECREF(globalEnv);

  baseNameSpaceEnv = (PySexpObject*)Sexp_new(&EnvironmentSexp_Type,
                                             Py_None, Py_None);
  sexpobj_ptr = Rpy_PreserveObject(R_EmptyEnv);
  Rpy_ReleaseObject(baseNameSpaceEnv->sObj->sexp);
  baseNameSpaceEnv->sObj = sexpobj_ptr;

  if (PyDict_SetItemString(d, "baseenv", 
                           (PyObject *)baseNameSpaceEnv) < 0)
  {
    Py_DECREF(baseNameSpaceEnv);
    return NULL;
  }
  Py_DECREF(baseNameSpaceEnv);

  rpy_R_NilValue = (PySexpObject*)Sexp_new(&Sexp_Type,
                                           Py_None, Py_None);
  if (PyDict_SetItemString(d, "R_NilValue", (PyObject *)rpy_R_NilValue) < 0)
  {
    Py_DECREF(rpy_R_NilValue);
    return NULL;
  }
  Py_DECREF(rpy_R_NilValue);

  R_PyObject_type_tag = (PySexpObject*)Sexp_new(&VectorSexp_Type,
						Py_None, Py_None);
  if (PyDict_SetItemString(d, "python_type_tag",
                           (PyObject *)R_PyObject_type_tag) < 0)
  {
    Py_DECREF(R_PyObject_type_tag);
    return NULL;
  }
  Py_DECREF(R_PyObject_type_tag);

  rinterface_unserialize = PyDict_GetItemString(d, "unserialize");
  return m;
}
