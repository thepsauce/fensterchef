/* This file is meant to be included by `configuration_parser.c`. */

static parser_error_t parse_startup_actions(Parser *parser);
static parser_error_t parse_assignment_association(Parser *parser);
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
        data_type_t data_type;
        /* offset within a `struct configuration` */
        size_t offset;
    } variables[7];
} labels[PARSER_LABEL_MAX] = {
    [PARSER_LABEL_STARTUP] = {
        "startup", parse_startup_actions, {
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_GENERAL] = {
        "general", NULL, {
        { "overlap-percentage", DATA_TYPE_INTEGER,
            offsetof(struct configuration, general.overlap_percentage) },
        { "root-cursor", DATA_TYPE_INTEGER,
            offsetof(struct configuration, general.root_cursor) },
        { "moving-cursor", DATA_TYPE_INTEGER,
            offsetof(struct configuration, general.moving_cursor) },
        { "horizontal-cursor", DATA_TYPE_INTEGER,
            offsetof(struct configuration, general.horizontal_cursor) },
        { "vertical-cursor", DATA_TYPE_INTEGER,
            offsetof(struct configuration, general.vertical_cursor) },
        { "sizing-cursor", DATA_TYPE_INTEGER,
            offsetof(struct configuration, general.sizing_cursor) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_ASSIGNMENT] = {
        "assignment", parse_assignment_association, {
        { "first-window-number", DATA_TYPE_INTEGER,
            offsetof(struct configuration, assignment.first_window_number) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_TILING] = {
        "tiling", NULL, {
        { "auto-split", DATA_TYPE_INTEGER,
            offsetof(struct configuration, tiling.auto_split) },
        { "auto-equalize", DATA_TYPE_INTEGER,
            offsetof(struct configuration, tiling.auto_equalize) },
        { "auto-fill-void", DATA_TYPE_INTEGER,
            offsetof(struct configuration, tiling.auto_fill_void) },
        { "auto-remove", DATA_TYPE_INTEGER,
            offsetof(struct configuration, tiling.auto_remove) },
        { "auto-remove-void", DATA_TYPE_INTEGER,
            offsetof(struct configuration, tiling.auto_remove_void) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_FONT] = {
        "font", NULL, {
        { "use-core-font", DATA_TYPE_INTEGER,
            offsetof(struct configuration, font.use_core_font) },
        { "name", DATA_TYPE_STRING,
            offsetof(struct configuration, font.name) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_BORDER] = {
        "border", NULL, {
        { "size", DATA_TYPE_INTEGER,
            offsetof(struct configuration, border.size) },
        { "color", DATA_TYPE_INTEGER,
            offsetof(struct configuration, border.color) },
        { "active-color", DATA_TYPE_INTEGER,
            offsetof(struct configuration, border.active_color) },
        { "focus-color", DATA_TYPE_INTEGER,
            offsetof(struct configuration, border.focus_color) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_GAPS] = {
        "gaps", NULL, {
        { "inner", DATA_TYPE_QUAD,
            offsetof(struct configuration, gaps.inner) },
        { "outer", DATA_TYPE_QUAD,
            offsetof(struct configuration, gaps.outer) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_NOTIFICATION] = {
        "notification", NULL, {
        { "duration", DATA_TYPE_INTEGER,
            offsetof(struct configuration, notification.duration) },
        { "padding", DATA_TYPE_INTEGER,
            offsetof(struct configuration, notification.padding) },
        { "border-size", DATA_TYPE_INTEGER,
            offsetof(struct configuration, notification.border_size) },
        { "border-color", DATA_TYPE_INTEGER,
            offsetof(struct configuration, notification.border_color) },
        { "foreground", DATA_TYPE_INTEGER,
            offsetof(struct configuration, notification.foreground) },
        { "background", DATA_TYPE_INTEGER,
            offsetof(struct configuration, notification.background) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_MOUSE] = {
        "mouse", parse_mouse_binding, {
        { "resize-tolerance", DATA_TYPE_INTEGER,
            offsetof(struct configuration, mouse.resize_tolerance) },
        { "modifiers", DATA_TYPE_INTEGER,
            offsetof(struct configuration, mouse.modifiers) },
        { "ignore-modifiers", DATA_TYPE_INTEGER,
            offsetof(struct configuration, mouse.ignore_modifiers) },
        /* null terminate the end */
        { NULL, 0, 0 } }
    },

    [PARSER_LABEL_KEYBOARD] = {
        "keyboard", parse_keyboard_binding, {
        { "modifiers", DATA_TYPE_INTEGER,
            offsetof(struct configuration, keyboard.modifiers) },
        { "ignore-modifiers", DATA_TYPE_INTEGER,
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
    [PARSER_LABEL_STARTUP] = {
        /* null terminate the end */
        { NULL, NULL }
    },

    [PARSER_LABEL_GENERAL] = {
        /* null terminate the end */
        { NULL, NULL }
    },

    [PARSER_LABEL_ASSIGNMENT] = {
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
