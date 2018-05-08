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
#include <R.h>
#include <Rinternals.h>
#include <Rdefines.h>
#include <R_ext/GraphicsEngine.h>
#include <R_ext/GraphicsDevice.h>

#include "_rinterface.h"
#include "r_utils.h"
#include "rpy_device.h"


PyDoc_STRVAR(module_doc,
             "Graphical devices for R. They can be interactive "
	     "(e.g., the X11 window that open during an interactive R session),"
	     " or not (e.g., PDF or PNG files).");

static inline void rpy_printandclear_error(void)
{
  PyObject* pythonerror = PyErr_Occurred();
  if (pythonerror != NULL) {
    /* All R actions should be stopped since the Python callback failed,
     and the Python exception raised up.*/
    /* FIXME: Print the exception in the meanwhile */
    PyErr_Print();
    PyErr_Clear();
  }
}

SEXP rpy_devoff(SEXP devnum, SEXP rho)
{
  SEXP c_R, call_R, res, fun_R;

#ifdef RPY_DEBUG_GRDEV
    printf("rpy_devoff(): checking 'rho'.\n");
#endif
  if(!isEnvironment(rho)) {
#ifdef RPY_DEBUG_GRDEV
    printf("rpy_devoff(): invalid 'rho'.\n");
#endif
    error("'rho' should be an environment\n");
  }

#ifdef RPY_DEBUG_GRDEV
  printf("rpy_devoff(): Looking for dev.off()...\n");
#endif
  PROTECT(fun_R = rpy2_findfun(install("dev.off"), rho));
  if (fun_R == R_UnboundValue)
    printf("dev.off() could not be found.\n");
#ifdef RPY_DEBUG_GRDEV
  printf("rpy_devoff(): found.\n");
#endif


  /* incantation to summon R */
  PROTECT(c_R = call_R = allocList(2));
  SET_TYPEOF(c_R, LANGSXP);
  SETCAR(c_R, fun_R);
  c_R = CDR(c_R);

  /* first argument is the device number to be closed */
  SETCAR(c_R, devnum);
  SET_TAG(c_R, install("which"));
  c_R = CDR(c_R);
  int error = 0;

#ifdef RPY_DEBUG_GRDEV
  printf("rpy_devoff(): R_tryEval()\n");
#endif

  PROTECT(res = R_tryEval(call_R, rho, &error));

#ifdef RPY_DEBUG_GRDEV
  printf("rpy_devoff(): unprotecting.\n");
#endif

  UNPROTECT(3);
  return res;
}


/* evaluate a call to a Python callback for the device */
static inline void rpy_GrDev_CallBack(pDevDesc dd, PyObject *name)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  PyObject *self = (PyObject *)dd->deviceSpecific;
  result = PyObject_CallMethodObjArgs(self, name, NULL);

  rpy_printandclear_error();

  Py_XDECREF(result);  
}


static PyObject *GrDev_close_name;

int
_GrDev_close(PyObject *self)
{
  PyObject *res;
  PyObject *tp, *v, *tb;
  int closed = 1;
  int is_zombie;
  /* if _GrDev_close() is called from a destructor
     (quite likely because of R's GEkillDevice()),
     we need to resurrect the object as calling close()
     can invoke arbitrary code (see Python's own iobase.c)
   */
  is_zombie = (Py_REFCNT(self) == 0);
  if (is_zombie) {
    ++Py_REFCNT(self);
  }
  PyErr_Fetch(&tp, &v, &tb);
  res = PyObject_GetAttrString(self, "closed");
  /* if the attribute "closed" does not exist, ignore */
  if (res == NULL)
    PyErr_Clear();
  else {
    closed = PyObject_IsTrue(res);
    Py_DECREF(res);
    if (closed == -1)
      PyErr_Clear();
  }
  if (closed == 0) {
    pDevDesc devdesc = ((PyGrDevObject *)self)->grdev;
    rpy_GrDev_CallBack(devdesc,
		       GrDev_close_name);
    /* FIXME: Shouldn't the result be checked ? */
  }
  PyErr_Restore(tp, v, tb);
  if (is_zombie) {
    if (--Py_REFCNT(self) != 0) {
      /* The object lives again. The following code is taken from
	 slot_tp_del in typeobject.c. */
      Py_ssize_t refcnt = Py_REFCNT(self);
      _Py_NewReference(self);
      Py_REFCNT(self) = refcnt;
      /* If Py_REF_DEBUG, _Py_NewReference bumped _Py_RefTotal, so
       * we need to undo that. */
      _Py_DEC_REFTOTAL;
      /* If Py_TRACE_REFS, _Py_NewReference re-added self to the object
       * chain, so no more to do there.
       * If COUNT_ALLOCS, the original decref bumped tp_frees, and
       * _Py_NewReference bumped tp_allocs:  both of those need to be
       * undone.
       */
#ifdef COUNT_ALLOCS
      --Py_TYPE(self)->tp_frees;
      --Py_TYPE(self)->tp_allocs;
#endif
      return -1;
    }
  }
  return 0;
}

static void rpy_Close(pDevDesc dd)
{
  printf("Closing device.\n");
  /* this callback is special because it can be called from
   a code path going through a Python destructor for the device */
  _GrDev_close(dd->deviceSpecific);
}


PyDoc_STRVAR(GrDev_close_doc,
             "Callback to implement: close the device."
	     "");
static PyObject* GrDev_close(PyObject *self)
{
  PyErr_Format(PyExc_NotImplementedError, "Device closing not implemented.");
  return NULL;
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_activate_name;
static void rpy_Activate(pDevDesc dd)
{
  rpy_GrDev_CallBack(dd, GrDev_activate_name);
}

PyDoc_STRVAR(GrDev_activate_doc,
             "Callback to implement: activation of the graphical device.");
static PyObject* GrDev_activate(PyObject *self)
{
  /* error("Not implemented."); */
  PyErr_Format(PyExc_NotImplementedError, "Device activation not implemented.");
  /* printf("done.\n"); */
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_deactivate_name;
static void rpy_Deactivate(pDevDesc dd)
{
  rpy_GrDev_CallBack(dd, GrDev_deactivate_name);
}

PyDoc_STRVAR(GrDev_deactivate_doc,
             "Callback to implement: deactivate the graphical device.");
static PyObject* GrDev_deactivate(PyObject *self)
{
  PyErr_Format(PyExc_NotImplementedError, "Device deactivation not implemented.");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_size_name;
static void rpy_Size(double *left, double *right, 
                     double *bottom, double *top,
                     pDevDesc dd)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */
  printf("FIXME: size(left=%f, right=%f, bottom=%f, top=%f)\n", 
         *left, *right, *bottom, *top);

  PyObject *self = (PyObject *)dd->deviceSpecific;

  //PyObject *lrbt = Py_BuildValue("(dddd)", *left, *right, *bottom, *top);
  //result = PyObject_CallMethodObjArgs(self, GrDev_size_name, 
  //                                    lrbt, NULL);
  result = PyObject_CallMethodObjArgs(self, GrDev_size_name, 
                                      NULL, NULL);
  rpy_printandclear_error();

  if (! PyTuple_Check(result) ) {
    PyErr_Format(PyExc_ValueError, "Callback 'size' should return a tuple.");
    rpy_printandclear_error();
  } else if (PyTuple_Size(result) != 4) {
    PyErr_Format(PyExc_ValueError, "Callback 'size' should return a tuple of length 4.");
    rpy_printandclear_error();
  } else {
    *left = PyFloat_AsDouble(PyTuple_GetItem(result, 0));
    *right = PyFloat_AsDouble(PyTuple_GetItem(result, 1));
    *bottom = PyFloat_AsDouble(PyTuple_GetItem(result, 2));
    *top = PyFloat_AsDouble(PyTuple_GetItem(result, 3));
  }
  //Py_DECREF(lrbt);
  Py_XDECREF(result);  
}

PyDoc_STRVAR(GrDev_size_doc,
             "Callback to implement: set the size of the graphical device.\n"
	     "The callback must return a tuple of 4 Python float (C double).\n"
	     "These could be:\n"
	     "left = 0\nright= <WindowWidth>\nbottom = <WindowHeight>\ntop=0\n"
	     );
static PyObject* GrDev_size(PyObject *self, PyObject *args)
{
  PyErr_Format(PyExc_NotImplementedError, 
               "Device size not implemented.\n"
               "[ expected signature is ((left, right, bottom, top)) \n]"
               "[ should return a tuple of length 4]");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_newpage_name;
static void rpy_NewPage(const pGEcontext gc, pDevDesc dd)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  /* FIXME give the callback access to gc */
  PyObject *self = (PyObject *)dd->deviceSpecific;
  result = PyObject_CallMethodObjArgs(self, GrDev_newpage_name, NULL);

  rpy_printandclear_error();

  Py_XDECREF(result);  
}

PyDoc_STRVAR(GrDev_newpage_doc,
             "Callback to implement: create a new page for the graphical device.\n"
	     "If the device can only handle one page, "
	     "the callback will have to eventually terminate clean an existing page.");
static PyObject* GrDev_newpage(PyObject *self, PyObject *args)
{
  printf("FIXME: newpage.\n");
  /* PyErr_Format(PyExc_NotImplementedError, "Not implemented."); */
  Py_INCREF(Py_None);
  printf("  done.\n");
  return Py_None;
}

static PyObject* GrDev_clip_name;
static void rpy_Clip(double x0, double x1, double y0, double y1, 
		     pDevDesc dd)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  PyObject *self = (PyObject *)dd->deviceSpecific;
  /* FIXME optimize ? (may be an array ?) */
  PyObject *py_x0 = PyFloat_FromDouble(x0);
  PyObject *py_x1 = PyFloat_FromDouble(x1);
  PyObject *py_y0 = PyFloat_FromDouble(y0);
  PyObject *py_y1 = PyFloat_FromDouble(y1);
  result = PyObject_CallMethodObjArgs(self, GrDev_clip_name, 
                                      py_x0, py_x1,
                                      py_y0, py_y1,
                                      NULL);

  rpy_printandclear_error();
  Py_DECREF(py_x0);
  Py_DECREF(py_x1);
  Py_DECREF(py_y0);
  Py_DECREF(py_y1);
  Py_XDECREF(result);
}

PyDoc_STRVAR(GrDev_clip_doc,
             "Callback to implement: clip the graphical device.\n"
	     "The callback method will receive 4 arguments (Python floats) corresponding "
	     "to the x0, x1, y0, y1 respectively.");
static PyObject* GrDev_clip(PyObject *self, PyObject *args)
{
  PyErr_Format(PyExc_NotImplementedError, "Device clip not implemented.");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_strwidth_name;
static double rpy_StrWidth(const char *str, const pGEcontext gc, pDevDesc dd)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  /* FIXME give the callback access to gc */
  PyObject *self = (PyObject *)dd->deviceSpecific;
#if (PY_VERSION_HEX < 0x03010000)
  PyObject *py_str = PyString_FromString(str);
#else
  PyObject *py_str = PyUnicode_FromString(str);
#endif
  result = PyObject_CallMethodObjArgs(self, GrDev_strwidth_name, py_str, NULL);

  rpy_printandclear_error();
  /*FIXME: only one of the two error should be printed. */
  if (!PyFloat_Check(result)) {
    PyErr_SetString(PyExc_TypeError,
		    "The value returned by strwidth must be a float");
  }
  rpy_printandclear_error();

  double r_res = PyFloat_AsDouble(result);
  Py_DECREF(py_str);
  Py_DECREF(result);  

  return r_res;
}

PyDoc_STRVAR(GrDev_strwidth_doc,
	     "Callback to implement: strwidth(text) -> width\n\n"
             "Width (in pixels) of a text when represented on the graphical device.\n"
	     "The callback will return a Python float (C double).");
static PyObject* GrDev_strwidth(PyObject *self, PyObject *args)
{
  PyErr_Format(PyExc_NotImplementedError, "Device strwidth not implemented.");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_text_name;
static void rpy_Text(double x, double y, const char *str,
                     double rot, double hadj,
		     const pGEcontext gc, pDevDesc dd)
{
    PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  PyObject *self = (PyObject *)dd->deviceSpecific;
  /* FIXME optimize ? */
  PyObject *py_x = PyFloat_FromDouble(x);
  PyObject *py_y = PyFloat_FromDouble(y);
#if (PY_VERSION_HEX < 0x03010000)
  PyObject *py_str = PyString_FromString(str);
#else
  PyObject *py_str = PyUnicode_FromString(str);
#endif
  PyObject *py_rot = PyFloat_FromDouble(rot);
  PyObject *py_hadj = PyFloat_FromDouble(hadj);
  /* FIXME pass gc ? */
  result = PyObject_CallMethodObjArgs(self, GrDev_text_name, 
                                      py_x, py_y,
                                      py_str, py_rot, py_hadj,
                                      NULL);

  rpy_printandclear_error();
  Py_DECREF(py_x);
  Py_DECREF(py_y);
  Py_DECREF(py_str);
  Py_DECREF(py_rot);
  Py_DECREF(py_hadj);
  Py_XDECREF(result);  
}

PyDoc_STRVAR(GrDev_text_doc,
             "Callback to implement: display text on the device.\n"
	     "The callback will receive the parameters:\n"
	     "x, y (position), string, rot (angle in degrees), hadj (some horizontal spacing parameter ?)");
static PyObject* GrDev_text(PyObject *self, PyObject *args)
{
  PyErr_Format(PyExc_NotImplementedError, "Device text not implemented.");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_rect_name;
static void rpy_Rect(double x0, double x1, double y0, double y1,
                     const pGEcontext gc, pDevDesc dd)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  PyObject *self = (PyObject *)dd->deviceSpecific;
  /* FIXME optimize ? */
  PyObject *py_x0 = PyFloat_FromDouble(x0);
  PyObject *py_x1 = PyFloat_FromDouble(x1);
  PyObject *py_y0 = PyFloat_FromDouble(y0);
  PyObject *py_y1 = PyFloat_FromDouble(y1);
  /* FIXME pass gc ? */
  result = PyObject_CallMethodObjArgs(self, GrDev_rect_name, 
                                      py_x0, py_x1,
                                      py_y0, py_y1,
                                      NULL);

  rpy_printandclear_error();
  Py_DECREF(py_x0);
  Py_DECREF(py_x1);
  Py_DECREF(py_y0);
  Py_DECREF(py_y1);
  Py_XDECREF(result);  
}

PyDoc_STRVAR(GrDev_rect_doc,
             "Callback to implement: draw a rectangle on the graphical device.\n"
	     "The callback will receive 4 parameters x0, x1, y0, y1.");
static PyObject* GrDev_rect(PyObject *self, PyObject *args)
{
  PyErr_Format(PyExc_NotImplementedError, "Device rect not implemented.");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_circle_name;
static void rpy_Circle(double x, double y, double r,
                       const pGEcontext gc, pDevDesc dd)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  PyObject *self = (PyObject *)dd->deviceSpecific;
  /* FIXME optimize ? */
  PyObject *py_x = PyFloat_FromDouble(x);
  PyObject *py_y = PyFloat_FromDouble(y);
  PyObject *py_r = PyFloat_FromDouble(r);
  /* FIXME pass gc ? */
  result = PyObject_CallMethodObjArgs(self, GrDev_circle_name, 
                                      py_x, py_y, py_r,
                                      NULL);

  rpy_printandclear_error();
  Py_DECREF(py_x);
  Py_DECREF(py_y);
  Py_DECREF(py_r);
  Py_XDECREF(result);  
}

PyDoc_STRVAR(GrDev_circle_doc,
             "Callback to implement: draw a circle on the graphical device.\n"
	     "The callback will receive the parameters x, y, radius");
static PyObject* GrDev_circle(PyObject *self, PyObject *args)
{
  PyErr_Format(PyExc_NotImplementedError, "Device circle not implemented.");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_line_name;
static void rpy_Line(double x1, double y1, 
                     double x2, double y2,
                     const pGEcontext gc, pDevDesc dd)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  PyObject *self = (PyObject *)dd->deviceSpecific;
  /* FIXME optimize ? */
  PyObject *py_x1 = PyFloat_FromDouble(x1);
  PyObject *py_y1 = PyFloat_FromDouble(y1);
  PyObject *py_x2 = PyFloat_FromDouble(x2);
  PyObject *py_y2 = PyFloat_FromDouble(y2);
  /* FIXME pass gc ? */
  result = PyObject_CallMethodObjArgs(self, GrDev_line_name, 
                                      py_x1, py_y1, py_x2, py_y2,
                                      NULL);

  rpy_printandclear_error();
  Py_DECREF(py_x1);
  Py_DECREF(py_y1);
  Py_DECREF(py_x2);
  Py_DECREF(py_y2);
  Py_DECREF(result);  
}

PyDoc_STRVAR(GrDev_line_doc,
             "Callback to implement: draw a line on the graphical device.\n"
	     "The callback will receive the arguments x1, y1, x2, y2.");
static PyObject* GrDev_line(PyObject *self, PyObject *args)
{
  PyErr_Format(PyExc_NotImplementedError, 
               "Device line not implemented.\n"
               "[expected signature is (x1, y1, x2, y2)]");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_polyline_name;
static void rpy_PolyLine(int n, double *x, double *y, 
                         const pGEcontext gc, pDevDesc dd)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  PyObject *self = (PyObject *)dd->deviceSpecific;
  #ifdef RPY_DEBUG_GRDEV
  printf("FIXME: PolyLine.\n");
  #endif
  /* FIXME optimize ? Yes ! MemoryViews.*/
  PyObject *py_x = PyTuple_New((Py_ssize_t)n);
  PyObject *py_y = PyTuple_New((Py_ssize_t)n);
  int i;
  for (i = 0; i < n; i++) {
    PyTuple_SET_ITEM(py_x, (Py_ssize_t)i, PyFloat_FromDouble(x[i]));
    PyTuple_SET_ITEM(py_y, (Py_ssize_t)i, PyFloat_FromDouble(y[i]));
  }
  /* FIXME pass gc ? */
  result = PyObject_CallMethodObjArgs(self, GrDev_polyline_name, 
                                      py_x, py_y,
                                      NULL);

  rpy_printandclear_error();
  Py_DECREF(py_x);
  Py_DECREF(py_y);
  Py_DECREF(result);  
}

PyDoc_STRVAR(GrDev_polyline_doc,
             "Callback to implement: draw a polyline on the graphical device.");
static PyObject* GrDev_polyline(PyObject *self, PyObject *args)
{
  PyErr_Format(PyExc_NotImplementedError, "Device polyline not implemented.");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_polygon_name;
static void rpy_Polygon(int n, double *x, double *y, 
                        const pGEcontext gc, pDevDesc dd)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  PyObject *self = (PyObject *)dd->deviceSpecific;
  /* FIXME optimize ? */
#ifdef RPY_DEBUG_GRDEV
  printf("FIXME: Polygon.\n");
#endif
  PyObject *py_n = PyLong_FromLong(n);
  /* FIXME: optimize by moving py_x and py_y to Python buffers */
  PyObject *py_x = PyTuple_New((Py_ssize_t)n);
  PyObject *py_y = PyTuple_New((Py_ssize_t)n);
  int i;
  for (i = 0; i < n; i++) {
    PyTuple_SET_ITEM(py_x, (Py_ssize_t)i, PyFloat_FromDouble(x[i]));
    PyTuple_SET_ITEM(py_y, (Py_ssize_t)i, PyFloat_FromDouble(y[i]));
  }
  /* FIXME pass gc ? */
  result = PyObject_CallMethodObjArgs(self, GrDev_polygon_name, 
                                      py_n, py_x, py_y,
                                      NULL);
  rpy_printandclear_error();
  Py_DECREF(py_x);
  Py_DECREF(py_y);
  Py_DECREF(py_n);
  Py_DECREF(result);  
}

PyDoc_STRVAR(GrDev_polygon_doc,
             "Callback to implement: draw a polygon on the graphical device.");
static PyObject* GrDev_polygon(PyObject *self, PyObject *args)
{
  PyErr_Format(PyExc_NotImplementedError, "Device polygon not implemented.");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_locator_name;
static Rboolean rpy_Locator(double *x, double *y, pDevDesc dd)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  PyObject *self = (PyObject *)dd->deviceSpecific;
  /* FIXME optimize ? */
#ifdef RPY_DEBUG_GRDEV
  printf("FIXME: Locator.\n");
#endif
  //PyObject *py_x = PyList_New(0);
  //PyObject *py_y = PyList_New(0);

  /* FIXME: pass gc ? */
  /* FIXME: test !dd->dev->locator before proceed ? */
  result = PyObject_CallMethodObjArgs(self, GrDev_locator_name, 
                                      //py_x, py_y,
                                      NULL);

  rpy_printandclear_error();

  if (! PyTuple_Check(result) ) {
    PyErr_Format(PyExc_ValueError, "Callback 'size' should return a tuple.");
    rpy_printandclear_error();
  } else if (PyTuple_Size(result) != 2) {
    PyErr_Format(PyExc_ValueError, "Callback 'size' should return a tuple of length 2.");
    rpy_printandclear_error();    
  } else {
    x[0] = PyFloat_AsDouble(PyTuple_GET_ITEM(result, 0));
    y[0] = PyFloat_AsDouble(PyTuple_GET_ITEM(result, 1));
    //int i;
      //for (i = 0; i < n; i++) {
      //x[i] = PyFloat_AsDouble(PyList_GET_ITEM(py_x, (Py_ssize_t)i));
      //y[i] = PyFloat_AsDouble(PyList_GET_ITEM(py_y, (Py_ssize_t)i));
      //}
  }

  Rboolean res_r = TRUE;
  printf("FIXME: return TRUE or FALSE");
  //Py_DECREF(py_x);
  //Py_DECREF(py_y);
  Py_DECREF(result);
  return res_r;
}

PyDoc_STRVAR(GrDev_locator_doc,
             "Callback to implement: locator on the graphical device.");
static PyObject* GrDev_locator(PyObject *self, PyObject *args)
{
  PyErr_Format(PyExc_NotImplementedError, "Device locator not implemented.");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_mode_name;
static void rpy_Mode(int mode, pDevDesc dd)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  PyObject *self = (PyObject *)dd->deviceSpecific;
#if (PY_VERSION_HEX < 0x03010000)
  PyObject *py_mode = PyInt_FromLong((long)mode);
#else
  PyObject *py_mode = PyLong_FromLong((long)mode);
#endif
  result = PyObject_CallMethodObjArgs(self, GrDev_mode_name, 
                                      py_mode,
                                      NULL);
  rpy_printandclear_error();
  Py_DECREF(py_mode);
  Py_DECREF(result);  
}

PyDoc_STRVAR(GrDev_mode_doc,
             "Callback to implement: mode of the graphical device.");
static PyObject* GrDev_mode(PyObject *self)
{
  PyErr_Format(PyExc_NotImplementedError, "Device mode not implemented.");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_metricinfo_name;
static void rpy_MetricInfo(int c, const pGEcontext gc, 
                           double* ascent, double* descent, double *width,
                           pDevDesc dd)
{
  PyObject *result;

  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */
  
  PyObject *self = (PyObject *)dd->deviceSpecific;
  /* FIXME optimize ? */
#ifdef RPY_DEBUG_GRDEV
  printf("FIXME: MetricInfo.\n");
#endif
#if (PY_VERSION_HEX < 0x03010000)
  PyObject *py_c = PyInt_FromLong((long)c);
#else
  PyObject *py_c = PyLong_FromLong((long)c);
#endif
  //PyObject *py_ascent = PyFloat_FromDouble(*ascent);
  //PyObject *py_descent = PyFloat_FromDouble(*descent);
  //PyObject *py_width = PyFloat_FromDouble(*width);
  /* FIXME pass gc ? */
  result = PyObject_CallMethodObjArgs(self, GrDev_metricinfo_name, 
                                      py_c,
                                      //py_ascent, py_descent, py_width,
                                      NULL);

  rpy_printandclear_error();


  if (! PyTuple_Check(result) ) {
    PyErr_Format(PyExc_ValueError, "Callback 'size' should return a tuple.");
    rpy_printandclear_error();
  } else if (PyTuple_Size(result) != 3) {
    PyErr_Format(PyExc_ValueError, "Callback 'metricinfo' should return a tuple of length 3.");
    rpy_printandclear_error();    
  } else {
    *ascent = PyFloat_AsDouble(PyTuple_GetItem(result, 0));
    *descent = PyFloat_AsDouble(PyTuple_GetItem(result, 1));
    *width = PyFloat_AsDouble(PyTuple_GetItem(result, 2));
  }
  Py_DECREF(py_c);
  //Py_DECREF(py_ascent);
  //Py_DECREF(py_descent);
  //Py_DECREF(py_width);
  Py_DECREF(result);

}

PyDoc_STRVAR(GrDev_metricinfo_doc,
             "Callback to implement: MetricInfo on the graphical device.");
static PyObject* GrDev_metricinfo(PyObject *self, PyObject *args)
{
  PyErr_Format(PyExc_NotImplementedError, "Device metricinfo not implemented.");
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* GrDev_getevent_name;
static SEXP rpy_GetEvent(SEXP rho, const char *prompt)
{
  SEXP r_res = R_NilValue;
  PyObject *result;

  pGEDevDesc dd = GEcurrentDevice();
  /* Restore the Python handler */
  /* FIXME */
  /* PyOS_setsig(SIGINT, python_sighandler); */

  PyObject *self = (PyObject *)dd->dev->deviceSpecific;
  /* FIXME optimize ? */
#ifdef RPY_DEBUG_GRDEV
  printf("FIXME: MetricInfo.\n");
#endif
#if (PY_VERSION_HEX < 0x03010000)
  PyObject *py_prompt = PyString_FromString(prompt);
#else
  PyObject *py_prompt = PyUnicode_FromString(prompt);
#endif
  /* FIXME pass gc ? */
  result = PyObject_CallMethodObjArgs(self, GrDev_getevent_name,
                                      py_prompt,
                                      NULL);

  rpy_printandclear_error();
  /* FIXME: check that the method only returns PySexp ? */
  printf("FIXME: check that only PySexp returned.\n");

  r_res = RPY_SEXP((PySexpObject *)result);
  /* FIXME: handle refcount and protection of the resulting r_res */
  printf("FIXME: handle refcount and protection of the resulting r_res");
  Py_DECREF(result);
  Py_DECREF(py_prompt);
  return r_res;
}

PyDoc_STRVAR(GrDev_getevent_doc,
             "Callback to implement: get event on the graphical device.");
static PyObject* GrDev_getevent(PyObject *self, PyObject *args)
{
  PyErr_Format(PyExc_NotImplementedError, "Device getevent not implemented.");
  Py_INCREF(Py_None);
  return Py_None;
}



void configureDevice(pDevDesc dd, PyObject *self)
{
  /* setup structure */
  dd->deviceSpecific = (void *) self;
  dd->close = rpy_Close;
  dd->activate = rpy_Activate;
  dd->deactivate = rpy_Deactivate;
  dd->size = rpy_Size;
  dd->newPage = rpy_NewPage;
  dd->clip = rpy_Clip;
  /* Next two are unused */
  dd->strWidth = rpy_StrWidth;
  dd->text = rpy_Text;
  dd->rect = rpy_Rect;
  dd->circle = rpy_Circle;
  dd->line = rpy_Line;
  dd->polyline = rpy_PolyLine;
  dd->polygon = rpy_Polygon;
  dd->locator = rpy_Locator;
  dd->mode = rpy_Mode;
  dd->metricInfo = rpy_MetricInfo;
  dd->getEvent = rpy_GetEvent;
  /* FIXME: initialization from self.attribute */
  dd->hasTextUTF8 = TRUE; /*PyObject_GetAttrString(self, ); */
  dd->wantSymbolUTF8 = TRUE;   /* FIXME: initialization from self.attribute */
  dd->strWidthUTF8 = rpy_StrWidth;
  dd->textUTF8 = rpy_Text;

  dd->left = 0;   /* FIXME: initialization from self.attribute */
  dd->right = 100;   /* FIXME: initialization from self.attribute */
  dd->bottom = 100;   /* FIXME: initialization from self.attribute */
  dd->top = 0;   /* FIXME: initialization from self.attribute */

  /* starting parameters */
  dd->startfont = 1; 
  dd->startps = 12.0; /* ps *  */
  dd->startcol = R_RGB(0, 0, 0);
  dd->startfill = R_TRANWHITE;
  dd->startlty = LTY_SOLID; 
  dd->startgamma = 1;
        
  /* dd->cra[0] = 0.9 * 12; */
  /* dd->cra[1] = 1.2 * 12; */
        
  /* character addressing offsets */
  dd->xCharOffset = 0.4900;
  dd->yCharOffset = 0.3333;
  dd->yLineBias = 0.1;

  /* inches per raster unit */
  dd->ipr[0] = 1;
  dd->ipr[1] = 1;

  /* device capabilities */
  dd->canClip = FALSE;
  dd->canHAdj = 0; /* text adjustment 0, 1, or 2 */
  dd->canChangeGamma = FALSE; /* FIXME: what is this ? */

  dd->canGenMouseDown = TRUE; /* can the device generate mousedown events */
  dd->canGenMouseMove = TRUE; /* can the device generate mousemove events */
  dd->canGenMouseUp = TRUE;   /* can the device generate mouseup events */
  
  dd->canGenKeybd = TRUE;     /* can the device generate keyboard events */
    
  dd->displayListOn = TRUE;
        
  /* finish */
}

static void
GrDev_clear(PyGrDevObject *self)
{
  /* FIXME */
  printf("FIXME: Clearing GrDev.\n");
  printf("  done.\n");
}

static void
GrDev_dealloc(PyGrDevObject *self)
{
  
#ifdef RPY_DEBUG_GRDEV
  printf("FIXME: Deallocating GrDev (device number %i).\n", RPY_DEV_NUM(self));
#endif
  pGEDevDesc dd = GEgetDevice(RPY_DEV_NUM(self)-1);
  /* Caution: GEkillDevice will call the method "close()" for the the device. */
  /* (See GrDev_close for details) */
  if (dd) GEkillDevice(dd);

#ifdef RPY_DEBUG_GRDEV
  printf("GrDevDealloc: PyMem_Free()\n");
#endif
  printf("--> skipping PyMem_Free(((PyGrDevObject *)self)->grdev) \n");
  //PyMem_Free(((PyGrDevObject *)self)->grdev);
#if (PY_VERSION_HEX < 0x03010000)
  self->ob_type->tp_free((PyObject*)self);
#else
  Py_TYPE(self)->tp_free((PyObject*)self);
#endif
#ifdef RPY_DEBUG_GRDEV
  printf("  done.\n");
#endif
}

static PyObject*
GrDev_repr(PyObject *self)
{
  pDevDesc devdesc = ((PyGrDevObject *)self)->grdev;
#if (PY_VERSION_HEX < 0x03010000)
  return PyString_FromFormat("<%s - Python:\%p / R graphical device:\%p>",
                             self->ob_type->tp_name,
                             self,
                             devdesc);
#else
  return PyUnicode_FromFormat("<%s - Python:\%p / R graphical device:\%p>",
			      Py_TYPE(self)->tp_name,
			      self,
			      devdesc);
#endif
}

static PyMethodDef GrDev_methods[] = {
  {"close", (PyCFunction)GrDev_close, METH_NOARGS,
   GrDev_close_doc},
  {"activate", (PyCFunction)GrDev_activate, METH_NOARGS,
   GrDev_activate_doc},
  {"deactivate", (PyCFunction)GrDev_deactivate, METH_NOARGS,
   GrDev_deactivate_doc},
  {"size", (PyCFunction)GrDev_size, METH_VARARGS,
   GrDev_size_doc},
  {"newpage", (PyCFunction)GrDev_newpage, METH_VARARGS,
   GrDev_newpage_doc},
  {"clip", (PyCFunction)GrDev_clip, METH_VARARGS,
   GrDev_clip_doc},
  {"strwidth", (PyCFunction)GrDev_strwidth, METH_VARARGS,
   GrDev_strwidth_doc},
  {"text", (PyCFunction)GrDev_text, METH_VARARGS,
   GrDev_text_doc},
  {"rect", (PyCFunction)GrDev_rect, METH_VARARGS,
   GrDev_rect_doc},
  {"circle", (PyCFunction)GrDev_circle, METH_VARARGS,
   GrDev_circle_doc},
  {"line", (PyCFunction)GrDev_line, METH_VARARGS,
   GrDev_line_doc},
  {"polyline", (PyCFunction)GrDev_polyline, METH_VARARGS,
   GrDev_polyline_doc},
  {"polygon", (PyCFunction)GrDev_polygon, METH_VARARGS,
   GrDev_polygon_doc},
  {"locator", (PyCFunction)GrDev_locator, METH_VARARGS,
   GrDev_locator_doc},
  {"mode", (PyCFunction)GrDev_mode, METH_VARARGS,
   GrDev_mode_doc},
  {"metricinfo", (PyCFunction)GrDev_metricinfo, METH_VARARGS,
   GrDev_metricinfo_doc},
  {"getevent", (PyCFunction)GrDev_getevent, METH_VARARGS,
   GrDev_getevent_doc},
  /* */
  {NULL, NULL}          /* sentinel */
};

RPY_GRDEV_BOOL_GETSET(hasTextUTF8,
		      "UTF8 capabilities of the device.")

RPY_GRDEV_BOOL_GETSET(wantSymbolUTF8,
		      "UTF8 capabilities of the device.")

PyDoc_STRVAR(GrDev_left_doc,
             "Left coordinate.");
static PyObject*
GrDev_left_get(PyObject *self)
{
  PyObject *res;
  res = PyFloat_FromDouble(((PyGrDevObject *)self)->grdev->left);
  return res;
}
static int
GrDev_left_set(PyObject *self, PyObject *value)
{
  RPY_GRDEV_FLOAT_SET(self, value, left);
}


PyDoc_STRVAR(GrDev_right_doc,
             "Right coordinate.");
static PyObject*
GrDev_right_get(PyObject *self)
{
  PyObject *res;
  res = PyFloat_FromDouble(((PyGrDevObject *)self)->grdev->right);
  return res;
}
static int
GrDev_right_set(PyObject *self, PyObject *value)
{
  RPY_GRDEV_FLOAT_SET(self, value, right);
}

PyDoc_STRVAR(GrDev_top_doc,
             "Top coordinate.");
static PyObject*
GrDev_top_get(PyObject *self)
{
  PyObject *res;
  res = PyFloat_FromDouble(((PyGrDevObject *)self)->grdev->top);
  return res;
}
static int
GrDev_top_set(PyObject *self, PyObject *value)
{
  RPY_GRDEV_FLOAT_SET(self, value, top);
}

PyDoc_STRVAR(GrDev_bottom_doc,
             "Bottom coordinate.");
static PyObject*
GrDev_bottom_get(PyObject *self)
{
  PyObject *res;
  res = PyFloat_FromDouble(((PyGrDevObject *)self)->grdev->bottom);
  return res;
}
static int
GrDev_bottom_set(PyObject *self, PyObject *value)
{
  RPY_GRDEV_FLOAT_SET(self, value, bottom);
}

RPY_GRDEV_BOOL_GETSET(canGenMouseDown,
             "Ability to generate mouse down events.")

RPY_GRDEV_BOOL_GETSET(canGenMouseMove,
             "Ability to generate mouse move events.")

RPY_GRDEV_BOOL_GETSET(canGenMouseUp,
             "Ability to generate mouse up events.")

RPY_GRDEV_BOOL_GETSET(canGenKeybd,
             "Ability to generate keyboard events.")

RPY_GRDEV_BOOL_GETSET(displayListOn,
             "Status of the display list.")

PyDoc_STRVAR(GrDev_devnum_doc,
             "Device number.");
static PyObject* GrDev_devnum_get(PyObject* self)
{
  PyObject* res;
  if ( RPY_DEV_NUM(self) == 0) {
    Py_INCREF(Py_None);
    res = Py_None;
  } else {
#if (PY_VERSION_HEX < 0x03010000)
    res = PyInt_FromLong((long)RPY_DEV_NUM(self));
#else
    res = PyLong_FromLong((long)RPY_DEV_NUM(self));
#endif
  }
  return res;

}

static PyObject *
rpydev_closed_get(PyObject *self, void *context)
{
  return PyBool_FromLong(PyObject_HasAttrString(self, "__GrDev_closed"));
}

 
static PyGetSetDef GrDev_getsets[] = {
  {"hasTextUTF8", 
   (getter)GrDev_hasTextUTF8_get,
   (setter)GrDev_hasTextUTF8_set,
   GrDev_hasTextUTF8_doc},
  {"wantSymbolUTF8", 
   (getter)GrDev_wantSymbolUTF8_get,
   (setter)GrDev_wantSymbolUTF8_set,
   GrDev_wantSymbolUTF8_doc},
  {"left", 
   (getter)GrDev_left_get,
   (setter)GrDev_left_set,
   GrDev_left_doc},
  {"right", 
   (getter)GrDev_right_get,
   (setter)GrDev_right_set,
   GrDev_right_doc},
  {"top", 
   (getter)GrDev_top_get,
   (setter)GrDev_top_set,
   GrDev_top_doc},
  {"bottom", 
   (getter)GrDev_bottom_get,
   (setter)GrDev_bottom_set,
   GrDev_bottom_doc},
  {"canGenMouseDown", 
   (getter)GrDev_canGenMouseDown_get,
   (setter)GrDev_canGenMouseDown_set,
   GrDev_canGenMouseDown_doc},
  {"canGenMouseMove", 
   (getter)GrDev_canGenMouseMove_get,
   (setter)GrDev_canGenMouseMove_set,
   GrDev_canGenMouseMove_doc},
  {"canGenMouseUp", 
   (getter)GrDev_canGenMouseUp_get,
   (setter)GrDev_canGenMouseUp_set,
   GrDev_canGenMouseUp_doc},
  {"canGenKeybd", 
   (getter)GrDev_canGenKeybd_get,
   (setter)GrDev_canGenKeybd_set,
   GrDev_canGenKeybd_doc},
  {"displayListOn", 
   (getter)GrDev_displayListOn_get,
   (setter)GrDev_displayListOn_set,
   GrDev_displayListOn_doc},
  /* */
  {"devnum",
   (getter)GrDev_devnum_get,
   NULL,
   GrDev_devnum_doc},
  {"closed",
   (getter)rpydev_closed_get, NULL, NULL},
  /* */
  {NULL, NULL, NULL, NULL}          /* sentinel */
};


static PyObject*
GrDev_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{

#ifdef RPY_DEBUG_GRDEV
  printf("FIXME: New GrDev\n");
#endif

  assert(type != NULL && type->tp_alloc != NULL);
  if (!rpy2_isinitialized()) {
    PyErr_Format(PyExc_RuntimeError, 
                 "R must be initialized before instances of GraphicalDevice can be created.");
    return NULL;
  }
  PyGrDevObject *self;
  self = (PyGrDevObject *)type->tp_alloc(type, 0);
  if (! self) {
    PyErr_NoMemory();
  }
  self->grdev = (pDevDesc)PyMem_Malloc(1 * sizeof(DevDesc));
  if (self->grdev == NULL) {
    PyErr_Format(PyExc_RuntimeError, 
                 "Could not allocate memory for an R device description.");
    return NULL;    
  }
#ifdef RPY_DEBUG_GRDEV
  printf("  done.\n");
#endif
  return(PyObject *)self;
}


static int
GrDev_init(PyObject *self, PyObject *args, PyObject *kwds)
{
#ifdef RPY_DEBUG_GRDEV
  printf("FIXME: Initializing GrDev\n");
#endif

  if (!rpy2_isinitialized()) {
    PyErr_Format(PyExc_RuntimeError, 
                 "R must be initialized before instances of GraphicalDevice can be created.");
    return -1;
  }

  if (R_CheckDeviceAvailableBool() != TRUE) {
    PyErr_Format(PyExc_RuntimeError, 
                 "Too many open R devices.");
    return -1;
  }

  pDevDesc dev = ((PyGrDevObject *)self)->grdev;

  configureDevice(dev, self);
  pGEDevDesc gdd = GEcreateDevDesc(dev);
#if (PY_VERSION_HEX < 0x03010000)
  GEaddDevice2(gdd, self->ob_type->tp_name);
#else
  GEaddDevice2(gdd, Py_TYPE(self)->tp_name);
#endif
  GEinitDisplayList(gdd);
  /* FIXME: protect device number ? */
  /* allocate memory for the pDevDesc structure ? */
  /* pDevDesc grdev = malloc(); */
  /* FIXME: handle allocation error */
  /* self->grdev = grdev; */
  
  return 0;
}




/*
 * Generic graphical device.
 */
PyDoc_STRVAR(GrDev_doc,
             "Python-defined graphical device for R.");

static PyTypeObject GrDev_Type = {
        /* The ob_type field must be initialized in the module init function
         * to be portable to Windows without using C++. */
#if (PY_VERSION_HEX < 0x03010000)
        PyObject_HEAD_INIT(NULL)
        0,                      /*ob_size*/
#else
	PyVarObject_HEAD_INIT(NULL, 0)
#endif
        "rpy2.rinterface.GraphicalDevice",   /*tp_name*/
        sizeof(PyGrDevObject),  /*tp_basicsize*/
        0,                      /*tp_itemsize*/
        /* methods */
        (destructor)GrDev_dealloc, /*tp_dealloc*/
        0,                      /*tp_print*/
        0,                      /*tp_getattr*/
        0,                      /*tp_setattr*/
#if (PY_VERSION_HEX < 0x03010000)
        0,                      /*tp_compare*/
#else
        0,                      /*tp_reserved*/
#endif
        GrDev_repr,             /*tp_repr*/
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
        GrDev_doc,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,/*(inquiry)Sexp_clear, tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        GrDev_methods,           /*tp_methods*/
        0,                      /*tp_members*/
        GrDev_getsets,            /*tp_getset*/
        0,                      /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        (initproc)GrDev_init,    /*tp_init*/
        0,                      /*tp_alloc*/
        GrDev_new,               /*tp_new*/
        0,                      /*tp_free*/
        0,                      /*tp_is_gc*/
#if (PY_VERSION_HEX < 0x03010000)
#else
	0,                      /*tp_bases*/
	0,                      /*tp_mro*/
	0,                      /*tp_cache*/
	0,                      /*tp_subclasses*/
	0                       /*tp_weaklist*/
#endif
};

/* Additional methods for RpyDevice */
static PyMethodDef rpydevice_methods[] = {
  {NULL,                NULL}           /* sentinel */
};



#ifndef PyMODINIT_FUNC  /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif

#if (PY_VERSION_HEX < 0x03010000)
#else
static struct PyModuleDef rpydevicemodule = {
   PyModuleDef_HEAD_INIT,
   "_rpy_device",           /* name of module */
   module_doc,             /* module documentation, may be NULL */
   -1,                     /* size of per-interpreter state of the module */
   NULL, NULL, NULL, NULL, NULL
 };
#endif

PyMODINIT_FUNC
#if (PY_VERSION_HEX < 0x03010000)
init_rpy_device(void)
#else
PyInit__rpy_device(void)
#endif
{
#if (PY_VERSION_HEX < 0x03010000)
  GrDev_close_name = PyString_FromString("close");
  GrDev_activate_name = PyString_FromString("activate");
  GrDev_deactivate_name = PyString_FromString("deactivate");
  GrDev_size_name = PyString_FromString("size");
  GrDev_newpage_name = PyString_FromString("newpage");
  GrDev_clip_name = PyString_FromString("clip");
  GrDev_strwidth_name = PyString_FromString("strwidth");
  GrDev_text_name = PyString_FromString("text");
  GrDev_rect_name = PyString_FromString("rect");
  GrDev_circle_name = PyString_FromString("circle");
  GrDev_line_name = PyString_FromString("line");
  GrDev_polyline_name = PyString_FromString("polyline");
  GrDev_polygon_name = PyString_FromString("polygon");
  GrDev_locator_name = PyString_FromString("locator");
  GrDev_mode_name = PyString_FromString("mode");
  GrDev_metricinfo_name = PyString_FromString("metricinfo");
  GrDev_getevent_name = PyString_FromString("getevent");
#else
  GrDev_close_name = PyUnicode_FromString("close");
  GrDev_activate_name = PyUnicode_FromString("activate");
  GrDev_deactivate_name = PyUnicode_FromString("deactivate");
  GrDev_size_name = PyUnicode_FromString("size");
  GrDev_newpage_name = PyUnicode_FromString("newpage");
  GrDev_clip_name = PyUnicode_FromString("clip");
  GrDev_strwidth_name = PyUnicode_FromString("strwidth");
  GrDev_text_name = PyUnicode_FromString("text");
  GrDev_rect_name = PyUnicode_FromString("rect");
  GrDev_circle_name = PyUnicode_FromString("circle");
  GrDev_line_name = PyUnicode_FromString("line");
  GrDev_polyline_name = PyUnicode_FromString("polyline");
  GrDev_polygon_name = PyUnicode_FromString("polygon");
  GrDev_locator_name = PyUnicode_FromString("locator");
  GrDev_mode_name = PyUnicode_FromString("mode");
  GrDev_metricinfo_name = PyUnicode_FromString("metricinfo");
  GrDev_getevent_name = PyUnicode_FromString("getevent");
#endif
  if (PyType_Ready(&GrDev_Type) < 0) {
#if (PY_VERSION_HEX < 0x03010000)
    return;
#else
    return NULL;
#endif
  }
  
  PyObject *m, *d;
#if (PY_VERSION_HEX < 0x03010000)
  m = Py_InitModule3("_rpy_device", rpydevice_methods, module_doc);
#else
  m = PyModule_Create(&rpydevicemodule);
#endif
  if (m == NULL) {
#if (PY_VERSION_HEX < 0x03010000)
    return;
#else
    return NULL;
#endif
  }

  if (import_rinterface() < 0)
#if (PY_VERSION_HEX < 0x03010000)
    return;
#else
    return NULL;
#endif

  d = PyModule_GetDict(m);
  PyModule_AddObject(m, "GraphicalDevice", (PyObject *)&GrDev_Type);  
#if (PY_VERSION_HEX < 0x03010000)
#else
  return m;
#endif
}
