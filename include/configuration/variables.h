#ifndef CONFIGURATION__VARIABES_H
#define CONFIGURATION__VARIABES_H

#include "configuration/data_type.h"

/* the variable map */
extern struct variable {
    /* type of the variable */
    data_type_t type;
    /* name of the variable */
    char *name;
    /* value of the variable */
    GenericData value;
} variables[2048];

/* Get the next slot where @name fits or a slot named @name.
 *
 * @return NULL if the variable does not exist and there is no free slot.
 */
struct variable *get_variable_slot(const char *name);

#endif
