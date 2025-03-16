#ifndef CONFIGURATION_PARSER_LABEL_H
#define CONFIGURATION_PARSER_LABEL_H

/* labels with form "[<name>]"
 *
 * NOTE: After editing this enum, also edit the `labels[]` array in
 * `configuration_parser.c`.
 * To add variables to the label, edit `variables[]` in `parse_line()` and also
 * add it to the configuration in `configuration.c` AND add a default option in
 * `default_configuration.c`.
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
