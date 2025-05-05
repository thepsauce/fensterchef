#include "configuration/configuration.h"
#include "x11_management.h"

/* the currently loaded configuration */
struct configuration configuration;

/* Get a key from button modifiers and a button index. */
struct configuration_button *find_configured_button(
        struct configuration *configuration, bool is_release,
        unsigned modifiers, unsigned button_index)
{
    struct configuration_button *button;

    /* remove the ignored modifiers */
    modifiers &= ~configuration->mouse.ignore_modifiers;
    /* remove all the button modifiers so that release events work properly */
    modifiers &= 0xff;

    /* find a matching button (the button AND modifiers must match up) */
    for (size_t i = 0; i < configuration->mouse.number_of_buttons; i++) {
        button = &configuration->mouse.buttons[i];
        if (button->modifiers == modifiers && button->index == button_index &&
                button->is_release == is_release) {
            return button;
        }
    }
    return NULL;
}

/* Grab the mouse bindings for a window so we receive MousePress/MouseRelease
 * events for it.
 *
 * This once was different were the buttons were grabbed on the root but this
 * led to weird bugs which I can not explain that caused odd behaviours when
 * replaying events.
 */
void grab_configured_buttons(Window window)
{
    struct configuration_button *button;

    /* ungrab all previous buttons so we can overwrite them */
    XUngrabButton(display, AnyButton, AnyModifier, window);

    for (size_t i = 0; i < configuration.mouse.number_of_buttons; i++) {
        button = &configuration.mouse.buttons[i];
        /* use every possible combination of modifiers we do not care about
         * so that when the user has CAPS LOCK for example, it does not mess
         * with mouse bindings
         */
        for (unsigned j = configuration.mouse.ignore_modifiers;
                true;
                j = ((j - 1) & configuration.mouse.ignore_modifiers)) {
            XGrabButton(display, button->index, (j | button->modifiers),
                    window, /* this is the window we grab the button for */
                    True, /* report all events with respect to `window` */
                    button->is_release ?
                        ButtonPressMask | ButtonReleaseMask :
                        ButtonPressMask,
                    /* SYNC means that pointer (mouse) events will be frozen
                     * until we issue a AllowEvents request; this allows us to
                     * make the decision to either drop the event or send it on
                     * to the actually pressed client
                     */
                    GrabModeSync,
                    /* do not freeze keyboard events */
                    GrabModeAsync,
                    None, /* no confinement of the pointer */
                    None /* no change of cursor */);

            if (j == 0) {
                break;
            }
        }
    }
}

/* Get a configured key from key modifiers and a key code. */
struct configuration_key *find_configured_key(
        struct configuration *configuration, bool is_release,
        unsigned modifiers, KeyCode key_code)
{
    struct configuration_key *key;

    modifiers &= ~configuration->keyboard.ignore_modifiers;

    /* find a matching key (the key symbol AND modifiers must match up) */
    for (size_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        key = &configuration->keyboard.keys[i];
        if (key->modifiers == modifiers && key->key_code == key_code &&
                key->is_release == is_release) {
            return key;
        }
    }
    return NULL;
}

/* Get a configured key from key modifiers and a key symbol. */
struct configuration_key *find_configured_key_by_key_symbol(
        struct configuration *configuration, bool is_release,
        unsigned modifiers, KeySym key_symbol)
{
    struct configuration_key *key;

    modifiers &= ~configuration->keyboard.ignore_modifiers;

    /* find a matching key (the key symbol AND modifiers must match up) */
    for (size_t i = 0; i < configuration->keyboard.number_of_keys; i++) {
        key = &configuration->keyboard.keys[i];
        if (key->key_symbol == NoSymbol) {
            continue;
        }
        if (key->modifiers == modifiers && key->key_symbol == key_symbol &&
                key->is_release == is_release) {
            return key;
        }
    }
    return NULL;
}

/* Grab the key bindings so we receive KeyPressed/KeyReleased events for them.
 */
void grab_configured_keys(void)
{
    Window root;
    struct configuration_key *key;

    root = DefaultRootWindow(display);

    /* remove all previously grabbed keys so that we can overwrite them */
    XUngrabKey(display, AnyKey, AnyModifier, root);

    /* re-compute any key codes */
    for (size_t i = 0; i < configuration.keyboard.number_of_keys; i++) {
        key = &configuration.keyboard.keys[i];
        if (key->key_symbol != NoSymbol) {
            key->key_code = XKeysymToKeycode(display, key->key_symbol);
        }
    }

    for (size_t i = 0; i < configuration.keyboard.number_of_keys; i++) {
        key = &configuration.keyboard.keys[i];
        /* iterate over all bit combinations */
        for (unsigned j = configuration.keyboard.ignore_modifiers;
                true;
                j = ((j - 1) & configuration.keyboard.ignore_modifiers)) {
            XGrabKey(display, key->key_code, (j | key->modifiers),
                    root, /* the window to grab the keys for */
                    True, /* report the event relative to the root */
                    /* do not freeze pointer (mouse) events */
                    GrabModeAsync,
                    /* do not freeze keyboard events */
                    GrabModeAsync);

            if (j == 0) {
                break;
            }
        }
    }
}
