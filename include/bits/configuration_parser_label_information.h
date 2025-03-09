/* This file is meant to be included by `configuration_parser.c`. */

static parser_error_t parse_startup_actions(Parser *parser);
static parser_error_t parse_mouse_binding(Parser *parser);
static parser_error_t parse_keyboard_binding(Parser *parser);

/* variables in the form <name> <value> */
static const struct configuration_parser_label_name {
    /* the string representation of the label */
    const char *name;
    /* special handling for a label */
    parser_error_t (*special_parser)(Parser *parser);
    /* the variables that can be defined in the label */
    struct configuration_parser_label_variable {
        /* name of the variable */
        const char *name;
        /* type of the variable */
        parser_data_type_t data_type;
        /* offset within a `struct configuration` */
        size_t offset;
    } variables[7];
} labels[PARSER_LABEL_MAX] = {
    [PARSER_LABEL_GENERAL] = {
        "general", NULL, {
        { "overlap-percentage", PARSER_DATA_TYPE_INTEGER,
            offsetof(struct configuration, general.overlap_percentage) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_STARTUP] = {
        "startup", parse_startup_actions, {
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_TILING] = {
        "tiling", NULL, {
        { "auto-fill-void", PARSER_DATA_TYPE_BOOLEAN,
            offsetof(struct configuration, tiling.auto_fill_void) },
        { "auto-remove-void", PARSER_DATA_TYPE_BOOLEAN,
            offsetof(struct configuration, tiling.auto_remove_void) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_FONT] = {
        "font", NULL, {
        { "name", PARSER_DATA_TYPE_STRING,
            offsetof(struct configuration, font.name) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_BORDER] = {
        "border", NULL, {
        { "size", PARSER_DATA_TYPE_INTEGER,
            offsetof(struct configuration, border.size) },
        { "color", PARSER_DATA_TYPE_COLOR,
            offsetof(struct configuration, border.color) },
        { "active-color", PARSER_DATA_TYPE_COLOR,
            offsetof(struct configuration, border.active_color) },
        { "focus-color", PARSER_DATA_TYPE_COLOR,
            offsetof(struct configuration, border.focus_color) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_GAPS] = {
        "gaps", NULL, {
        { "inner", PARSER_DATA_TYPE_QUAD,
            offsetof(struct configuration, gaps.inner) },
        { "outer", PARSER_DATA_TYPE_QUAD,
            offsetof(struct configuration, gaps.outer) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_NOTIFICATION] = {
        "notification", NULL, {
        { "duration", PARSER_DATA_TYPE_INTEGER,
            offsetof(struct configuration, notification.duration) },
        { "padding", PARSER_DATA_TYPE_INTEGER,
            offsetof(struct configuration, notification.padding) },
        { "border-size", PARSER_DATA_TYPE_INTEGER,
            offsetof(struct configuration, notification.border_size) },
        { "border-color", PARSER_DATA_TYPE_COLOR,
            offsetof(struct configuration, notification.border_color) },
        { "foreground", PARSER_DATA_TYPE_COLOR,
            offsetof(struct configuration, notification.foreground) },
        { "background", PARSER_DATA_TYPE_COLOR,
            offsetof(struct configuration, notification.background) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_MOUSE] = {
        "mouse", parse_mouse_binding, {
        { "resize-tolerance", PARSER_DATA_TYPE_INTEGER,
            offsetof(struct configuration, mouse.resize_tolerance) },
        { "modifiers", PARSER_DATA_TYPE_MODIFIERS,
            offsetof(struct configuration, mouse.modifiers) },
        { "ignore-modifiers", PARSER_DATA_TYPE_MODIFIERS,
            offsetof(struct configuration, mouse.ignore_modifiers) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_KEYBOARD] = {
        "keyboard", parse_keyboard_binding, {
        { "modifiers", PARSER_DATA_TYPE_MODIFIERS,
            offsetof(struct configuration, keyboard.modifiers) },
        { "ignore-modifiers", PARSER_DATA_TYPE_MODIFIERS,
            offsetof(struct configuration, keyboard.ignore_modifiers) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },
};

static parser_error_t merge_default_mouse(Parser *parser);
static parser_error_t merge_default_keyboard(Parser *parser);

/* the parser commands are in the form <command> <arguments> */
static const struct configuration_parser_command {
    /* the name of the command */
    const char *name;
    /* the procedure to execute (parses and executes the command) */
    parser_error_t (*procedure)(Parser *parser);
} commands[PARSER_LABEL_MAX][2] = {
    [PARSER_LABEL_GENERAL] = {
        /* null terminate the end */
        { NULL, NULL }
    },

    [PARSER_LABEL_STARTUP] = {
        /* null terminate the end */
        { NULL, NULL }
    },

    [PARSER_LABEL_TILING] = {
        /* null terminate the end */
        { NULL, NULL }
    },

    [PARSER_LABEL_FONT] = {
        /* null terminate the end */
        { NULL, NULL }
    },

    [PARSER_LABEL_BORDER] = {
        /* null terminate the end */
        { NULL, NULL }
    },

    [PARSER_LABEL_GAPS] = {
        /* null terminate the end */
        { NULL, NULL }
    },

    [PARSER_LABEL_NOTIFICATION] = {
        /* null terminate the end */
        { NULL, NULL }
    },

    [PARSER_LABEL_MOUSE] = {
        { "merge-default", merge_default_mouse },
        /* null terminate the end */
        { NULL, NULL }
    },

    [PARSER_LABEL_KEYBOARD] = {
        { "merge-default", merge_default_keyboard },
        /* null terminate the end */
        { NULL, NULL }
    },
};
