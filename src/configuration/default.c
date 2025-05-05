#include "configuration/default.h"

/* the settings of the default configuration */
const struct configuration default_configuration;

/* Merge parts of the default configuration into the current configuration. */
void merge_default_configuration(unsigned flags)
{
    (void) flags;
}
