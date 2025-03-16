#include <ctype.h>

#include <xcb/xcb.h>

#include <X11/keysym.h>
#include <X11/Xlib.h> // XStringToKeysym

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
        unsigned digit;
    } digits[] = {
        [0] = { "one", 1 },
        [1] = { "two", 2 },
        [2] = { "nine", 9 },
        [3] = { "four", 4 },
        [4] = { "eight", 8 },
        [5] = { "five", 5 },
        [6] = { "", 0 },
        [7] = { "seven", 7 },
        [8] = { "six", 6 },
        [9] = { "three", 3 },
        [10] = { "zero", 0 }
    };

    if (string[0] == '\0') {
        return XCB_NONE;
    }

    /* the aformentioned hash function */
    const unsigned digits_hash = (tolower(string[0]) ^ tolower(string[1])) / 3;
    if (digits_hash < SIZE(digits) &&
            strcasecmp(digits[digits_hash].name, string) == 0) {
        return XK_0 + digits[digits_hash].digit;
    }

    return XStringToKeysym(string);
}

