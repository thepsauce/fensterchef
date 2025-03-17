#ifndef CONFIGURATION_PARSER_LABEL_H
#define CONFIGURATION_PARSER_LABEL_H

/* labels with form "[<name>]"
 *
 * NOTE: Labels should not be added here but in "generate/fensterchef.labels".
 */
typedef enum configuration_parser_label {
    PARSER_LABEL_STARTUP,
    PARSER_LABEL_GENERAL,
    PARSER_LABEL_ASSIGNMENT,
    PARSER_LABEL_TILING,
    PARSER_LABEL_FONT,
    PARSER_LABEL_BORDER,
    PARSER_LABEL_GAPS,
    PARSER_LABEL_NOTIFICATION,
    PARSER_LABEL_MOUSE,
    PARSER_LABEL_KEYBOARD,

    PARSER_LABEL_MAX
} parser_label_t;

#endif
