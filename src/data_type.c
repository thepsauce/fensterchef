#include <string.h>

#include "data_type.h"
#include "utility.h"

/* the size in bytes of each data type */
size_t data_type_sizes[DATA_TYPE_MAX] = {
    [DATA_TYPE_VOID] = 0,
    [DATA_TYPE_BOOLEAN] = sizeof(((GenericData*) 0)->boolean),
    [DATA_TYPE_STRING] = sizeof(((GenericData*) 0)->string),
    [DATA_TYPE_INTEGER] = sizeof(((GenericData*) 0)->integer),
    [DATA_TYPE_QUAD] = sizeof(((GenericData*) 0)->quad),
    [DATA_TYPE_COLOR] = sizeof(((GenericData*) 0)->color),
    [DATA_TYPE_MODIFIERS] = sizeof(((GenericData*) 0)->modifiers),
    [DATA_TYPE_CURSOR] = sizeof(((GenericData*) 0)->cursor),
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
    case DATA_TYPE_BOOLEAN:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_QUAD:
    case DATA_TYPE_COLOR:
    case DATA_TYPE_MODIFIERS:
    case DATA_TYPE_CURSOR:
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
    case DATA_TYPE_BOOLEAN:
    case DATA_TYPE_VOID:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_QUAD:
    case DATA_TYPE_COLOR:
    case DATA_TYPE_MODIFIERS:
    case DATA_TYPE_CURSOR:
        break;

    /* not a real data type */
    case DATA_TYPE_MAX:
        break;
    }
}
