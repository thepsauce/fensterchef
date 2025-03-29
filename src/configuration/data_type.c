#include <string.h>

#include "configuration/data_type.h"
#include "utility.h"

/* the size in bytes of each data type */
size_t data_type_sizes[DATA_TYPE_MAX] = {
    [DATA_TYPE_VOID] = 0,
    [DATA_TYPE_STRING] = sizeof(((GenericData*) 0)->string),
    [DATA_TYPE_INTEGER] = sizeof(((GenericData*) 0)->integer),
    [DATA_TYPE_QUAD] = sizeof(((GenericData*) 0)->quad),
};

/* Duplicates given @value deeply into itself. */
void duplicate_data_value(data_type_t type, GenericData *data)
{
    switch (type) {
    /* do a copy of the string */
    case DATA_TYPE_STRING:
        data->string = (utf8_t*) xstrdup((char*) data->string);
        break;

    /* these have no data that needs to be deep copied */
    case DATA_TYPE_VOID:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_QUAD:
        break;

    /* not a real data type */
    case DATA_TYPE_MAX:
        break;
    }
}

/* Free the resources the given data value occupies. */
void clear_data_value(data_type_t type, GenericData *data)
{
    switch (type) {
    /* free the string value */
    case DATA_TYPE_STRING:
        free(data->string);
        break;

    /* these have no data that needs to be cleared */
    case DATA_TYPE_VOID:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_QUAD:
        break;

    /* not a real data type */
    case DATA_TYPE_MAX:
        break;
    }
}
