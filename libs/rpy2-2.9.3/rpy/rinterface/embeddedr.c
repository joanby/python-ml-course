#include "embeddedr.h"

/* Helper variable to store R's status
 */
const unsigned int const RPY_R_INITIALIZED = 0x01;
const unsigned int const RPY_R_BUSY = 0x02;
/* Initial status is 0 */
static unsigned int embeddedR_status = 0;

/* An environment to keep R objects preserved by rpy2 */
static SEXP RPY_R_PreciousEnv = NULL;
static PyObject *Rpy_R_Precious;

static inline void embeddedR_setlock(void) {
  embeddedR_status = embeddedR_status | RPY_R_BUSY;
}
static inline void embeddedR_freelock(void) {
  embeddedR_status = embeddedR_status ^ RPY_R_BUSY;
}
static inline unsigned int rpy_has_status(unsigned int status) {
  return (embeddedR_status & status) == status;
}

/*FIXME: this is not thread safe (calls to R not using
         locking). Can this is be a source of errors ?
*/
static void SexpObject_clear(SexpObject *sexpobj)
{
  if (sexpobj->pycount <= 0) {
    printf("Warning: clearing an R object with a refcount <= zero.\n");
  }

  if ((sexpobj->sexp) != R_NilValue) {
    /* R objects that needs to be preserved from garbage collection */
    if (RPY_R_PreciousEnv == NULL) {
      /* Use the R "precious list" */
      R_ReleaseObject(sexpobj->sexp);
    } else {
      /* Use the environment */
      static char *name_buf;
      if (name_buf == NULL) {
	/* Initialize with the number of characters required for an hexadecimal
	   representation of a pointer*/
	name_buf = (char *)calloc(sizeof(name_buf)*2+2+1, sizeof(char)) ; 
      }
      sprintf(name_buf, "%p", (void *)(sexpobj->sexp));
      
      SEXP res = rpy2_remove(Rf_mkString(name_buf), 
			     RPY_R_PreciousEnv,
			     Rf_ScalarLogical(FALSE));
      //Rf_defineVar(name_r, sexpobj->sexp, RPY_R_PreciousEnv);
    }
    PyMem_Free(sexpobj);
  }

/*   sexpobj->count--; */

/* #ifdef RPY_VERBOSE */
/*   printf("R:%p -- sexp count is %i...",  */
/*          sexpobj->sexp, sexpobj->count); */
/* #endif */
/*   if (((*sexpobj).count == 0) && (*sexpobj).sexp) { */
/* #ifdef RPY_VERBOSE */
/*     printf("freeing SEXP resources..."); */
/* #endif  */

/* /\*     if (sexpobj->sexp != R_NilValue) { *\/ */
/* /\* #ifdef RPY_DEBUG_PRESERVE *\/ */
/* /\*       printf("  PRESERVE -- Sexp_clear: R_ReleaseObject -- %p ",  *\/ */
/* /\*              sexpobj->sexp); *\/ */
/* /\*       preserved_robjects -= 1; *\/ */
/* /\*       printf("-- %i\n", preserved_robjects); *\/ */
/* /\* #endif  *\/ */
/* /\*       int preserve_status = Rpy_ReleaseObject(sexpobj->sexp); *\/ */
/* /\*       if (preserve_status == -1) { *\/ */
/* /\* 	PyErr_Print(); *\/ */
/* /\* 	PyErr_Clear(); *\/ */
/* /\*       } *\/ */
/* /\*     } *\/ */
/*     /\* self->ob_type->tp_free((PyObject*)self); *\/ */
/* #ifdef RPY_VERBOSE */
/*     printf("done.\n"); */
/* #endif  */
/*   }   */
}

static void SexpObject_CObject_destroy(PyObject *rpycapsule)
{
  SexpObject *sexpobj_ptr = (SexpObject *)(PyCapsule_GetPointer(rpycapsule,
								"rpy2.rinterface._rinterface.SEXPOBJ_C_API"));
  SexpObject_clear(sexpobj_ptr);
}

/* Keep track of R objects preserved by rpy2 
   Return NULL on failure (a Python exception being set) 
 */
static SexpObject* Rpy_PreserveObject(SEXP object) {
  /* PyDict can be confused if an error has been raised.
     We put aside the exception if the case, to restore it at the end.
     FIXME: this situation can occur because of presumed shortcomings
     in the overall design of rpy2.
   */
  int reset_error_state = 0; 
  PyObject *ptype, *pvalue, *ptraceback;

  if (PyErr_Occurred()) {
    reset_error_state = 1;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
  }

  PyObject *key = PyLong_FromVoidPtr((void *)object);
  PyObject *capsule = PyDict_GetItem(Rpy_R_Precious, key);
  SexpObject *sexpobj_ptr;
  /* capsule is a borrowed reference */
  if (capsule == NULL) {
    /* The R object is not yet tracked by rpy2 so we:
       - create a new SexpObject. 
       - create a capsule for it
       - put the capsule in the tracking dictionary
    */
    sexpobj_ptr = (SexpObject *)PyMem_Malloc(1 * sizeof(SexpObject));
    if (! sexpobj_ptr) {
      PyErr_NoMemory();
      return NULL;
    }
    sexpobj_ptr->pycount = 1;
    sexpobj_ptr->sexp = object;
    capsule = PyCapsule_New((void *)(sexpobj_ptr),
			    "rpy2.rinterface._rinterface.SEXPOBJ_C_API",
			    SexpObject_CObject_destroy);
    if (PyDict_SetItem(Rpy_R_Precious, key, capsule) == -1) {
      Py_DECREF(key);
      Py_DECREF(capsule);
      return NULL;
    }
    Py_DECREF(capsule);
    if (object != R_NilValue) {
      /* R objects that needs to be preserved from garbage collection */
      if (RPY_R_PreciousEnv == NULL) {
	/* Use the R "precious list" */
	R_PreserveObject(object);
      } else {
	/* Use an enclosing environment instead of R's "precious list" 
	   to protect the object from garbage collection */
	static char *name_buf;
	if (name_buf == NULL) {
	  /* Initialize with the number of characters required for an hexadecimal
	     representation of a pointer*/
	  name_buf = (char *)calloc(sizeof(name_buf)*2+2+1, sizeof(char)) ; 
	}
	sprintf(name_buf, "%p", (void *)object);
	SEXP name_r = Rf_install(name_buf);
	Rf_defineVar(name_r, object, RPY_R_PreciousEnv);
      }
    }
  } else {
    /* Reminder: capsule is a borrowed reference */
    sexpobj_ptr = (SexpObject *)(PyCapsule_GetPointer(capsule,
						      "rpy2.rinterface._rinterface.SEXPOBJ_C_API"));
    if (sexpobj_ptr != NULL) {
      sexpobj_ptr->pycount++;
    }
  }
  Py_DECREF(key);
  
  if (reset_error_state) {
    if (PyErr_Occurred()) {
      PyErr_Print();
      PyErr_Clear();
    }
    PyErr_Restore(ptype, pvalue, ptraceback);
  }
  return sexpobj_ptr;
} 

/* static int Rpy_PreserveObject(SEXP object) { */
/* R_ReleaseObject(RPY_R_Precious); */
/* PROTECT(RPY_R_Precious); */
/* RPY_R_Precious = CONS(object, RPY_R_Precious); */
/* UNPROTECT(1); */
/* R_PreserveObject(RPY_R_Precious); */
/* } */

static int Rpy_ReleaseObject(SEXP object) {
  /* PyDict can be confused if an error has been raised.
     We put aside the exception if the case, to restore it at the end.
     FIXME: this situation can occur because of presumed shortcomings
     in the overall design of rpy2.
   */
  int reset_error_state = 0; 
  PyObject *ptype, *pvalue, *ptraceback; 
  if (PyErr_Occurred()) {
    reset_error_state = 1;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
  }

  PyObject *key = PyLong_FromVoidPtr((void *)object);
  PyObject *capsule = PyDict_GetItem(Rpy_R_Precious, key);
  /* capsule is a borrowed reference */
  if (capsule == NULL) {
    /* FIXME: should all rpy2 proxy objects have an associated capsule ?
     *        If yes, why are we here ?
     */
    printf("Warning: the rpy2 object we are trying to release has no associated capsule.\n");
    if (reset_error_state) {
      PyErr_Restore(ptype, pvalue, ptraceback);
      printf("Restoring an earlier exception.\n");
      printf("Error:Trying to release object ID %ld while not preserved\n",
	     PyLong_AsLong(key));
    } else {
      PyErr_Format(PyExc_KeyError, 
		   "Trying to release object ID %ld while not preserved\n",
		   PyLong_AsLong(key));
    }
    Py_DECREF(key);
    return -1;
  } 

  SexpObject *sexpobj_ptr = (SexpObject *)(PyCapsule_GetPointer(capsule,
								"rpy2.rinterface._rinterface.SEXPOBJ_C_API"));
  if (sexpobj_ptr == NULL) {
    if (reset_error_state) {
      if (PyErr_Occurred()) {
	PyErr_Print();
      }
      PyErr_Restore(ptype, pvalue, ptraceback);
    }
    Py_DECREF(key);
    return -1;
  }
  int res = 0;

  switch (sexpobj_ptr->pycount) {
  case 0:
    if (object != R_NilValue) {
      res = -1;
      PyErr_Format(PyExc_ValueError,
		   "Preserved object ID %ld with a count of zero\n", 
		   PyLong_AsLong(key));
      Py_DECREF(key);
      return res;
    }
    break;
  case 1:
    /* By deleting the capsule from the dictionary, the count of the SexpObject
      will go down by one, reach zero, and the release of the R object 
      will be performed. */
    if (object == R_NilValue) {
      sexpobj_ptr->pycount--;
    } else {
      res = PyDict_DelItem(Rpy_R_Precious, key);
      if (res == -1)
	PyErr_Format(PyExc_ValueError,
		     "Occured while deleting preserved object ID %ld\n",  
		     PyLong_AsLong(key));
    }
    break;
  case 2:
    /* When the refcount is exactly 2, we could have the following possible
     * situations:
     * A- 1 PySexpObject, 1 SexpObject in a capsule
     * B- 2 SexpObject in a capsule
     * C- 2 PySexObject
     * Only A is effectively possible because each PySexpObject has
     * an associated capsule (rules out C) and each capsule is unique
     * for a given SEXP (rules out B).
     * In addition to that, the reference counting in rpy2 is independent
     * from Python's reference counting. This is means that in the situation A/
     * above we can have n pointers to the PySexpObject and m pointers
     * to the SexpObject. 
     */
    //  ob_refcnt;
    /* if (PyLong_AsLong(key) == 0) { */
    /*   printf("Count 2 for: 0\n"); */
    /*   break; */
    /* } */
    sexpobj_ptr->pycount--;
    /* if (object == R_NilValue) { */
    /*   sexpobj_ptr->count--; */
    /* } else { */
    /*   //printf("-->use to delete %ld here\n", PyLong_AsLong(key)); */
    /*   res = PyDict_DelItem(Rpy_R_Precious, key); */
    /*   if (res == -1) */
    /*   	PyErr_Format(PyExc_ValueError, */
    /*   		     "Occured while deleting preserved object ID %ld\n", */
    /*   		     PyLong_AsLong(key)); */
    /* } */
    break;
  default:
    sexpobj_ptr->pycount--;
    break;
  }
  
  Py_DECREF(key);
  if (reset_error_state) {
    if (PyErr_Occurred()) {
      PyErr_Print();
    }
    PyErr_Restore(ptype, pvalue, ptraceback);
  }
  return res;
}
  /* SEXP parentnode, node; */
  /* Py_ssize_t res = -1; */
  /* if (isNull(RPY_R_Precious)) { */
  /*   return res; */
  /* } */
  /* res++; */
  /* if (object == CAR(RPY_R_Precious)) { */
  /*   RPY_R_Precious = CDR(RPY_R_Precious); */
  /*   return res; */
  /* } */
  /* parentnode = RPY_R_Precious; */
  /* node = CDR(RPY_R_Precious); */
  /* while (!isNull(node)) { */
  /*   res++; */
  /*   if (object == CAR(node)) { */
  /*     SETCDR(parentnode, CDR(node)); */
  /*     return res; */
  /*   } */
  /*   parentnode = node; */
  /*   node = CDR(node); */
  /* } */

PyDoc_STRVAR(Rpy_ProtectedIDs_doc,
             "Return a tuple of pairs with each: \n"
	     "- an R ID for the objects protected from R's garbage collection by rpy2\n"
	     "- the number of rpy2 objects protecting that R object from collection.\n\n"
	     "The R ID is a memory pointer for the R-defined C-structure "
	     "containing all information about the R object. It is available "
	     "from an rpy2 object through the read-only attribute `rid`.");
/* Return a tuple with IDs of R objects protected by rpy2 and counts */
static PyObject* Rpy_ProtectedIDs(PyObject *self) {
  PyObject *key, *capsule;
  Py_ssize_t pos = 0;
  PyObject *ids = PyTuple_New(PyDict_Size(Rpy_R_Precious));
  Py_ssize_t pos_ids = 0;
  PyObject *id_count;
  SexpObject *sexpobject_ptr;

  while (PyDict_Next(Rpy_R_Precious, &pos, &key, &capsule)) {
    id_count = PyTuple_New(2);
    Py_INCREF(key);
    PyTuple_SET_ITEM(id_count, 0, key);
    sexpobject_ptr = (SexpObject *)(PyCapsule_GetPointer(capsule,
							 "rpy2.rinterface._rinterface.SEXPOBJ_C_API"));
    PyTuple_SET_ITEM(id_count, 1, PyLong_FromLong(sexpobject_ptr->pycount));
    PyTuple_SET_ITEM(ids, pos_ids, id_count);
    pos_ids++;
  }
  return ids;  
}

/* return 0 on success, -1 on failure (and set an exception) */
static inline int Rpy_ReplaceSexp(PySexpObject *pso, SEXP rObj) {
  SexpObject *sexpobj_ptr = Rpy_PreserveObject(rObj);
  //printf("target: %zd\n", sexpobj_ptr->count);
  if (sexpobj_ptr == NULL) {
    return -1;
  }
  //printf("orig: %zd\n", pso->sObj->count);
  SEXP sexp = pso->sObj->sexp;
  pso->sObj = sexpobj_ptr;
  int res = Rpy_ReleaseObject(sexp);
  return res;
}


PyDoc_STRVAR(EmbeddedR_isInitialized_doc,
             "is_initialized() -> bool\n"
	     ""
	     "Return whether R is initialized.");

static PyObject*
EmbeddedR_isInitialized(void) {
  if (rpy2_isinitialized()) {
    Py_INCREF(Py_True);
    return Py_True;
  } else {
    Py_INCREF(Py_False);
    return Py_False;
  }
}
