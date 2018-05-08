#ifndef _RPY_PRIVATE_EMBEDDEDR_H_
#define _RPY_PRIVATE_EMBEDDEDR_H_

#include  <Rdefines.h>

extern const unsigned int const RPY_R_INITIALIZED;
extern const unsigned int const RPY_R_BUSY;

/* Representation of R objects (instances) as instances in Python.
 */

static PyObject* Rpy_R_Precious;
static SEXP RPY_R_PreciousEnv;
static void embeddedR_setlock(void);
static void embeddedR_freelock(void);
static unsigned int rpy_has_status(unsigned int);
static void SexpObject_clear(SexpObject *sexpobj);
static void SexpObject_CObject_destroy(PyObject *rpycapsule);
static unsigned int embeddedR_status;
static SexpObject* Rpy_PreserveObject(SEXP object);
static int Rpy_ReleaseObject(SEXP object);
static inline int Rpy_ReplaceSexp(PySexpObject *pso, SEXP rObj);
#endif


