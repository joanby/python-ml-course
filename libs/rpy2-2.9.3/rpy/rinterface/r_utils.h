#ifndef RPY_RU_H
#define RPY_RU_H

#include <Rdefines.h>
#include <Rversion.h>

SEXP rpy2_serialize(SEXP object, SEXP rho);
SEXP rpy2_unserialize(SEXP connection, SEXP rho);
SEXP rpy2_remove(SEXP symbol, SEXP environment, SEXP rho);

SEXP rpy2_list_attr(SEXP sexp);

SEXP rpy2_lang2str(SEXP sexp, SEXPTYPE t);

SEXP externallymanaged_vector(SEXPTYPE rtype, void *array, int length);

SEXP rpy2_newenv(SEXP hash, SEXP parent, SEXP size);

int rpy2_isinitialized(void);
int rpy2_setinitialized(void);

typedef struct {
  int rfree;
  void *array;
} ExternallyManagedVector;

SEXP rpy2_findfun(SEXP symbol, SEXP rho);


#define __RPY_RSVN_SWITCH_VERSION__ 134914

#endif
