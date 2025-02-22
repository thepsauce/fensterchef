#include <xcb/xcb.h>

#include <X11/Xlib.h> // XStringToKeysym

/* TODO: extract necessary code out of XStringToKeysym, could be fun */

/* Translate a string to a key symbol. */
xcb_keysym_t string_to_keysym(const char *string)
{
    return XStringToKeysym(string);
}

