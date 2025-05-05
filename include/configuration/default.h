#ifndef CONFIGURATION__DEFAULT_H
#define CONFIGURATION__DEFAULT_H

#include "configuration/configuration.h"

/* the settings of the default configuration */
extern const struct configuration default_configuration;

/* TODO: more flags */
#define DEFAULT_CONFIGURATION_MERGE_ALL 0xff

/* Merge parts of the default configuration into the current configuration. */
void merge_default_configuration(unsigned flags);

#endif
