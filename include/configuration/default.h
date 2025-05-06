#ifndef CONFIGURATION__DEFAULT_H
#define CONFIGURATION__DEFAULT_H

/**
 * The default configuration of fensterchef is included within its binary.
 */

#include "configuration/configuration.h"

/* the settings of the default configuration */
extern const struct configuration default_configuration;

#define DEFAULT_CONFIGURATION_MERGE_KEY_BINDINGS (1 << 0)
#define DEFAULT_CONFIGURATION_MERGE_BUTTON_BINDINGS (1 << 1)
#define DEFAULT_CONFIGURATION_MERGE_FONT (1 << 2)
#define DEFAULT_CONFIGURATION_MERGE_CURSOR (1 << 3)
#define DEFAULT_CONFIGURATION_MERGE_SETTINGS (1 << 4)
#define DEFAULT_CONFIGURATION_MERGE_ALL 0xff

/* Merge parts of the default configuration into the current configuration. */
void merge_default_configuration(unsigned flags);

#endif
