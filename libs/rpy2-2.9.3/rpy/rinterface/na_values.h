#ifndef _RPY_PRIVATE_NAVALUES_H_
#define _RPY_PRIVATE_NAVALUES_H_

#ifndef _RPY_RINTERFACE_MODULE_
#error na_values.h should not be included
#endif

static PyTypeObject NAInteger_Type;
static PyTypeObject NAReal_Type;
static PyTypeObject NAComplex_Type;
static PyTypeObject NALogical_Type;
static PyTypeObject NACharacter_Type;

typedef union
{
  double value;
  unsigned int word[2];
} ieee_double;

#ifdef RPY_BIGENDIAN
static const ieee_double NAREAL_IEEE = {.word = {0x7ff00000, 1954}};
#else
static const ieee_double NAREAL_IEEE = {.word = {1954, 0x7ff00000}};
#endif


#endif


