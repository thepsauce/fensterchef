#include "binding.h"
#include "log.h"
#include "x11_synchronize.h"

/* additional modifiers on top of the ones used in binding setting */
static unsigned modifiers_addition = Mod4Mask;

/* the modifiers to ignore */
static unsigned modifiers_ignore = LockMask | Mod2Mask;

/* The mouse bindings.  The key to this map are button indexes. */
static struct internal_button_binding {
    /* if the button is grabbed on the X server */
    bool is_grabbed;
    /* if this binding is triggered on a release */
    bool is_release;
    /* if the event should pass through to the window the event happened in */
    bool is_transparent;
    /* the button modifiers */
    unsigned modifiers;
    /* the actions to execute */
    struct action_list actions;
    /* another variation using the same button index */
    struct internal_button_binding *next;
} *button_bindings[BUTTON_MAX - BUTTON_MIN];

/* The key bindings.  The key to this map are key codes. */
static struct internal_key_binding {
    /* if the key is grabbed on the X server */
    bool is_grabbed;
    /* if this key binding is triggered on a release */
    bool is_release;
    /* the key modifiers */
    unsigned modifiers;
    /* the key symbol, may be `NoSymbol` */
    KeySym key_symbol;
    /* the actions to execute */
    struct action_list actions;
    /* another variation using the same key code */
    struct internal_key_binding *next;
} *key_bindings[KEYCODE_MAX - KEYCODE_MIN];

/* Set the modifiers to add to the ones passed into the set binding function. */
inline void set_additional_modifiers(unsigned modifiers)
{
    modifiers_addition = modifiers;
}

/* Set the modifiers to ignore. */
inline void set_ignored_modifiers(unsigned modifiers)
{
    modifiers_ignore = modifiers;
}

/*****************************
 * Button binding management *
 *****************************/

/* Find a button binding that is already set. */
static inline struct internal_button_binding *find_button_binding(
        bool is_release, unsigned modifiers, button_t button)
{
    struct internal_button_binding *binding;

    binding = button_bindings[button - BUTTON_MIN];
    for (; binding != NULL; binding = binding->next) {
        /* check if the binding is the same */
        if (binding->is_release == is_release &&
                binding->modifiers == modifiers) {
            break;
        }
    }

    return binding;
}

/* Set the speficied button binding. */
void set_button_binding(const struct button_binding *button_binding)
{
    unsigned index;
    unsigned modifiers;
    struct internal_button_binding *binding;

    index = button_binding->button;

    if (index >= BUTTON_MAX) {
        LOG_ERROR("invalid button index %u\n",
                index);
        return;
    }

    modifiers = button_binding->modifiers;
    modifiers |= modifiers_addition;

    binding = find_button_binding(button_binding->is_release, modifiers, index);

    if (binding == NULL) {
        ALLOCATE_ZERO(binding, 1);
        binding->next = button_bindings[index - BUTTON_MIN];
        button_bindings[index - BUTTON_MIN] = binding;

        binding->is_release = button_binding->is_release;
        binding->modifiers = modifiers;

        synchronization_flags |= SYNCHRONIZE_BUTTON_BINDING;
    }

    /* clear any old binded actions */
    clear_action_list(&binding->actions);

    binding->is_transparent = button_binding->is_transparent;
    binding->actions = button_binding->actions;
    duplicate_action_list(&binding->actions);
}

/* Run the specified key binding. */
void run_button_binding(Time event_time, bool is_release,
        unsigned modifiers, button_t button)
{
    struct internal_button_binding *binding;

    /* remove the ignored modifiers */
    modifiers &= ~modifiers_ignore;
    /* remove all the button modifiers so that release events work properly */
    modifiers &= 0xff;

    binding = find_button_binding(is_release, modifiers, button);
    if (binding != NULL && binding->actions.number_of_items > 0) {
        LOG("running actions: %A\n",
                &binding->actions);
        run_action_list(&binding->actions);
        if (binding->is_transparent) {
            XAllowEvents(display, ReplayPointer, event_time);
        }
    }
}

/* Clear a button binding. */
void clear_button_binding(bool is_release,
        unsigned modifiers, button_t button)
{
    struct internal_button_binding *binding;

    binding = find_button_binding(is_release, modifiers, button);
    if (binding != NULL) {
        clear_action_list(&binding->actions);
        synchronization_flags |= SYNCHRONIZE_BUTTON_BINDING;
    }
}


/* Clear all configured button bindings. */
void clear_button_bindings(void)
{
    struct internal_button_binding *binding;

    for (int i = BUTTON_MIN; i < BUTTON_MAX; i++) {
        binding = button_bindings[i - BUTTON_MIN];
        for (; binding != NULL; binding = binding->next) {
            clear_action_list(&binding->actions);
        }
    }
    synchronization_flags |= SYNCHRONIZE_BUTTON_BINDING;
}

/**************************
 * Key binding management *
 **************************/

/* Find a button binding that is already set. */
static inline struct internal_key_binding *find_key_binding(
        bool is_release, unsigned modifiers, KeyCode key_code)
{
    struct internal_key_binding *binding;

    binding = key_bindings[key_code - KEYCODE_MIN];
    for (; binding != NULL; binding = binding->next) {
        /* check if the binding is the same */
        if (binding->is_release == is_release &&
                binding->modifiers == modifiers) {
            break;
        }
    }
    return binding;
}

/* Set the speficied key binding. */
void set_key_binding(const struct key_binding *key_binding)
{
    KeyCode key_code;
    unsigned modifiers;
    struct internal_key_binding *binding;

    if (key_binding->key_symbol != NoSymbol) {
        key_code = XKeysymToKeycode(display, key_binding->key_symbol);
    } else {
        key_code = key_binding->key_code;
    }

    modifiers = key_binding->modifiers;
    modifiers |= modifiers_addition;

    binding = find_key_binding(key_binding->is_release, modifiers, key_code);

    if (binding == NULL) {
        ALLOCATE_ZERO(binding, 1);
        binding->next = key_bindings[key_code - KEYCODE_MIN];
        key_bindings[key_code - KEYCODE_MIN] = binding;

        binding->is_release = key_binding->is_release;
        binding->modifiers = modifiers;
        binding->key_symbol = key_binding->key_symbol;

        synchronization_flags |= SYNCHRONIZE_KEY_BINDING;
    }

    /* clear any old binded actions */
    clear_action_list(&binding->actions);

    binding->actions = key_binding->actions;
    duplicate_action_list(&binding->actions);
}

/* Run the specified key binding. */
void run_key_binding(bool is_release, unsigned modifiers, KeyCode key_code)
{
    struct internal_key_binding *binding;

    /* remove the ignored modifiers */
    modifiers &= ~modifiers_ignore;

    binding = find_key_binding(is_release, modifiers, key_code);
    if (binding != NULL && binding->actions.number_of_items > 0) {
        LOG("running actions: %A\n",
                &binding->actions);
        run_action_list(&binding->actions);
    }
}

/* Clear a key binding. */
void clear_key_binding(bool is_release, unsigned modifiers, KeyCode key_code)
{
    struct internal_key_binding *binding;

    binding = find_key_binding(is_release, modifiers, key_code);
    if (binding != NULL) {
        clear_action_list(&binding->actions);
        synchronization_flags |= SYNCHRONIZE_KEY_BINDING;
    }
}

/* Clear all configured key bindings. */
void clear_key_bindings(void)
{
    struct internal_key_binding *binding;

    for (int i = KEYCODE_MIN; i < KEYCODE_MAX; i++) {
        binding = key_bindings[i - KEYCODE_MIN];
        for (; binding != NULL; binding = binding->next) {
            clear_action_list(&binding->actions);
        }
    }
    synchronization_flags |= SYNCHRONIZE_KEY_BINDING;
}

/* Resolve all key symbols in case the mapping changed. */
void resolve_all_key_symbols(void)
{
    struct internal_key_binding *previous, *next, *binding;
    KeyCode key_code;

    for (int i = KEYCODE_MIN; i < KEYCODE_MAX; i++) {
        previous = NULL;
        binding = key_bindings[i - KEYCODE_MIN];
        for (; binding != NULL; binding = next) {
            /* we grab this binding after the double for loop has ended */
            binding->is_grabbed = true;

            /* this `next` mechanism needs to be employed because the current
             * binding might move away
             */
            next = binding->next;

            if (binding->key_symbol == NoSymbol ||
                    /* set the key code and check it it changed */
                    (key_code = XKeysymToKeycode(display, binding->key_symbol),
                        key_code == i)) {
                previous = binding;
                continue;
            }

            LOG_DEBUG("key code of key symbol %ld (%c) has changed\n",
                    binding->key_symbol, binding->key_symbol);

            /* unlink and... */
            if (previous != NULL) {
                previous->next = binding->next;
                LOG_DEBUG("keycode %d still has more before\n", 
                        i);
            } else {
                key_bindings[i - KEYCODE_MIN] = binding->next;
                if (binding->next == NULL) {
                    LOG_DEBUG("getting rid of keycode %d\n",
                            i);
                } else {
                    LOG_DEBUG("keycode %d still has more following\n",
                            i);
                }
            }

            /* ...relink */
            binding->next = key_bindings[key_code - KEYCODE_MIN];
            key_bindings[key_code - KEYCODE_MIN] = binding;
            LOG_DEBUG("linking into keycode %d\n",
                    key_code);
        }
    }

    /* re-grab all bindings */
    grab_configured_keys();
}

/**************************
 * Server synchronization *
 **************************/

/* Grab the button bindings for a window so we receive MousePress/MouseRelease
 * events for it.
 *
 * This once was different were the buttons were grabbed on the root but this
 * led to weird bugs which I can not explain that caused odd behaviours when
 * replaying events.
 */
void grab_configured_buttons(Window window)
{
    const struct internal_button_binding *binding;

    /* ungrab all previous buttons so we can overwrite them */
    XUngrabButton(display, AnyButton, AnyModifier, window);

    for (button_t i = BUTTON_MIN; i < BUTTON_MAX; i++) {
        binding = button_bindings[i - BUTTON_MIN];
        for (; binding != NULL; binding = binding->next) {
            /* ignore bindings with no action */
            if (binding->actions.number_of_items == 0) {
                continue;
            }

            /* use every possible combination of modifiers we do not care about
             * so that when the user has CAPS LOCK for example, it does not mess
             * with button bindings
             */
            for (unsigned j = modifiers_ignore;
                    true;
                    j = ((j - 1) & modifiers_ignore)) {
                XGrabButton(display, i, (j | binding->modifiers),
                        /* report all events with respect to this window */
                        window, True,
                        /* check if release is needed */
                        binding->is_release ?
                            ButtonPressMask | ButtonReleaseMask :
                            ButtonPressMask,
                        /* SYNC means that pointer (mouse) events will be frozen
                         * until we issue a AllowEvents request; this allows us
                         * to make the decision to either drop the event or send
                         * it on to the actually pressed client
                         */
                        GrabModeSync,
                        /* do not freeze keyboard events */
                        GrabModeAsync,
                        /* no confinement of the pointer and no cursor change */
                        None, None);

                if (j == 0) {
                    break;
                }
            }
        }
    }
}

/* Grab the key bindings so we receive KeyPress/KeyRelease events for them. */
void grab_configured_keys(void)
{
    Window root;
    const struct internal_key_binding *binding;

    root = DefaultRootWindow(display);

    /* remove all previously grabbed keys so that we can overwrite them */
    XUngrabKey(display, AnyKey, AnyModifier, root);

    for (int i = KEYCODE_MIN; i < KEYCODE_MAX; i++) {
        binding = key_bindings[i - KEYCODE_MIN];
        for (; binding != NULL; binding = binding->next) {
            /* ignore bindings with no action */
            if (binding->actions.number_of_items == 0) {
                continue;
            }

            /* iterate over all bit combinations */
            for (unsigned j = modifiers_ignore;
                    true;
                    j = ((j - 1) & modifiers_ignore)) {
                XGrabKey(display, i, (j | binding->modifiers),
                        /* report all events with respect to the root */
                        root, True,
                        /* do not freeze pointer and keyboard events */
                        GrabModeAsync, GrabModeAsync);

                if (j == 0) {
                    break;
                }
            }
        }
    }
}
