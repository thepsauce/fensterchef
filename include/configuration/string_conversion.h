#ifndef CONFIGURATION__STRING_CONVERSION_H
#define CONFIGURATION__STRING_CONVERSION_H

#include <stdbool.h>
#include <stdint.h>

#include <xcb/xcb.h>

/* Translate a string like "Shift" to a modifier bit. */
int string_to_modifier(const char *string, uint16_t *output);

/* Translate a string like "Button1" to a button index. */
int string_to_button(const char *string, xcb_button_t *index);

/* Translate a string like "false" to a boolean value. */
int string_to_boolean(const char *string, bool *output);

/* Translate a constant which can be any of the above functions to an integer.
 *
 * The string can also be a cursor constant.
 */
int string_to_constant(const char *string, int32_t *output);

#endif
