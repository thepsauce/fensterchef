#ifndef BITS__BINDING_H
#define BITS__BINDING_H

#include <X11/X.h>

#include <stdbool.h>

/* Note: The actual values on the X11 server may be even more restricted but
 * these work every time.
 */
/* minimum value a keycode can have */
#define KEYCODE_MIN 8
/* maximum value of a key code (exclusive) */
#define KEYCODE_MAX 256

/* list of all button indexes */
typedef enum {
    BUTTON_NONE,

    BUTTON_MIN,

    BUTTON_LEFT = BUTTON_MIN,
    BUTTON_MIDDLE,
    BUTTON_RIGHT,

    BUTTON_WHEEL_UP,
    BUTTON_WHEEL_DOWN,
    BUTTON_WHEEL_LEFT,
    BUTTON_WHEEL_RIGHT,

    BUTTON_X1,
    BUTTON_X2,
    BUTTON_X3,
    BUTTON_X4,
    BUTTON_X5,
    BUTTON_X6,
    BUTTON_X7,
    BUTTON_X8,

    BUTTON_MAX,
} button_t;

/* a button binding structure to pass to `set_button_binding()` */
struct button_binding {
    /* if this key binding is triggered on a release */
    bool is_release;
    /* if the event should pass through to the window the event happened in */
    bool is_transparent;
    /* the key modifiers */
    unsigned modifiers;
    /* the button index */
    button_t button;
    /* the actions to execute */
    struct action_list actions;
};

/* a key binding structure to pass to `set_key_binding()` */
struct key_binding {
    /* if this key binding is triggered on a release */
    bool is_release;
    /* the key modifiers */
    unsigned modifiers;
    /* the key symbol, may be `NoSymbol` */
    KeySym key_symbol;
    /* the key code to use */
    KeyCode key_code;
    /* the actions to execute */
    struct action_list actions;
};

#endif
