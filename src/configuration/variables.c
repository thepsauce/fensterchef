#include <string.h>

#include "configuration/variables.h"

#include "utility.h"

/* the variable list */
struct variable variables[2048];

/* the number of set variables */
static uint32_t Variable_count;

/* Get the next slot where @name fits or a slot named @name. */
struct variable *get_variable_slot(const char *name)
{
    const uint32_t fnv_prime = 0x01000193;
    const uint32_t fnv_offset_basis = 0x811c9dc5;

    uint32_t hash;
    uint32_t step = 0;

    /* use FNV-1 hashing to get an initial hash value */
    hash = fnv_offset_basis;
    for (const char *i = name; i[0] != '\0'; i++) {
        hash *= fnv_prime;
        hash ^= i[0] - 'a';
    }

    /* fine tune the hash using quadratic probing */
    do {
        hash += (step * step + step) / 2;
        hash %= SIZE(variables);
        step++;
    } while (variables[hash].name != NULL &&
            strcmp(variables[hash].name, name) != 0);

    if (variables[hash].name == NULL &&
            /* we allow `variables` to be filled by 83% */
            Variable_count * 5 / 4 > SIZE(variables)) {
        return NULL;
    }

    return &variables[hash];
}
