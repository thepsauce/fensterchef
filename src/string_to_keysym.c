#include <ctype.h>

#include <xcb/xcb.h>

#include <X11/keysym.h>
#include <X11/Xlib.h> // XStringToKeysym()

#include <string.h> // strcmp()

#include "utility.h"

/* TODO: extract necessary code out of XStringToKeysym, could be fun */

/* Translate a string to a key symbol. */
xcb_keysym_t string_to_keysym(const char *string)
{
    /* map from string to digit value, they are arranged in this way to match
     * the hash function following `digits_hash = `
     * there is one gap within the map, this is the best hash function I found
     * quickly
     */
    const struct {
        const char *name;
        xcb_keysym_t key_symbol;
    } digits[] = {
        [0] = { "one", XK_1 },
        [1] = { "two", XK_2 },
        [2] = { "nine", XK_9 },
        [3] = { "four", XK_4 },
        [4] = { "eight", XK_8 },
        [5] = { "five", XK_5 },
        [6] = { "", XK_0 },
        [7] = { "seven", XK_7 },
        [8] = { "six", XK_6 },
        [9] = { "three", XK_3 },
        [10] = { "zero", XK_0 }
    };

    if (string[0] == '\0') {
        return XCB_NONE;
    }

    /* the aformentioned hash function */
    const unsigned digits_hash = (string[0] ^ string[1]) / 3;
    if (digits_hash < SIZE(digits) &&
            strcmp(digits[digits_hash].name, string) == 0) {
        return digits[digits_hash].key_symbol;
    }

    return XStringToKeysym(string);
}

