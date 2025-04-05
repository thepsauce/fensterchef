#include <string.h>

#include "configuration/string_conversion.h"
#include "cursor.h"
#include "utility.h"

/* Translate a string like "Shift" to a modifier bit. */
int string_to_modifier(const char *string, unsigned *output)
{
    /* conversion of string to modifier mask */
    static const struct modifier_string {
        /* the string representation of the modifier */
        const char *name;
        /* the modifier bit */
        uint16_t modifier;
    } modifier_strings[] = {
        { "None", 0 },

        { "Shift", ShiftMask },
        { "Lock", LockMask },
        { "CapsLock", LockMask },
        { "Ctrl", ControlMask },
        { "Control", ControlMask },

        /* common synonyms for some modifiers */
        { "Alt", Mod1Mask },
        { "Super", Mod4Mask },

        { "Mod1", Mod1Mask },
        { "Mod2", Mod2Mask },
        { "Mod3", Mod3Mask },
        { "Mod4", Mod4Mask },
        { "Mod5", Mod5Mask }
    };

    for (unsigned i = 0; i < SIZE(modifier_strings); i++) {
        if (strcmp(modifier_strings[i].name, string) == 0) {
            *output = modifier_strings[i].modifier;
            return OK;
        }
    }
    return ERROR;
}

/* Translate a string like "Button1" to a button index. */
int string_to_button(const char *string, unsigned *output)
{
    /* conversion from string to button index */
    static const struct button_string {
        /* the string representation of the button */
        const char *name;
        /* the button index */
        unsigned button_index;
    } button_strings[] = {
        /* buttons can also be Button<integer> to directly address the index */
        { "LButton", 1 },
        { "LeftButton", 1 },

        { "MButton", 2 },
        { "MiddleButton", 2 },

        { "Rbutton", 3 },
        { "RightButton", 3 },

        { "ScrollUp", 4 },
        { "WheelUp", 4 },

        { "ScrollDown", 5 },
        { "WheelDown", 5 },

        { "ScrollLeft", 6 },
        { "WheelLeft", 6 },

        { "ScrollRight", 7 },
        { "WheelRight", 7 },
        /* the first index of the X buttons */
#define FIRST_X_BUTTON 8
#define NUMBER_OF_X_BUTTONS (UINT8_MAX - FIRST_X_BUTTON)
        /* X buttons (extra buttons on the mouse usually) going from X1 (8) to
         * X247 (255), they have their own handling and are not listed here
         */
    };

    unsigned index = 0;

    /* parse indexes starting with "X" */
    if (string[0] == 'X') {
        string++;
        while (isdigit(string[0])) {
            index *= 10;
            index += string[0] - '0';
            if (index > NUMBER_OF_X_BUTTONS) {
                return ERROR;
            }
            string++;
        }

        if (index == 0) {
            return ERROR;
        }

        *output = FIRST_X_BUTTON + index - 1;
        return OK;
    /* parse indexes starting with "Button" */
    } else if (string[0] == 'B' &&
            string[1] == 'u' &&
            string[2] == 't' &&
            string[3] == 't' &&
            string[4] == 'o' &&
            string[5] == 'n') {
        string += strlen("Button");
        while (isdigit(string[0])) {
            index *= 10;
            index += string[0] - '0';
            if (index > UINT8_MAX) {
                return ERROR;
            }
            string++;
        }
        *output = index;
        return OK;
    }

    for (unsigned i = 0; i < SIZE(button_strings); i++) {
        if (strcmp(button_strings[i].name, string) == 0) {
            *output = button_strings[i].button_index;
            return OK;
        }
    }
    return ERROR;
}

/* Translate a string like "false" to a boolean value. */
int string_to_boolean(const char *string, bool *output)
{
    /* these strings are sorted in accordance to the below hash function */
    const char *strings[] = {
        "on",
        "no",
        "true",
        "false",
        "yes",
        "off",
    };

    if (string[0] == '\0' || string[1] == '\0') {
        return ERROR;
    }

    /* I looked at many hash functions of the form `((S * X) >> Y) & 0x7` and
     * this one has a good property that the boolean value can also be implied
     * very easily through the hash value
     */
    const unsigned hash = ((string[1] * 57) >> 7) & 0x7;
    if (hash < SIZE(strings) && strcmp(strings[hash], string) == 0) {
        *output = !(hash & 1);
        return OK;
    }

    return ERROR;
}

/* Translate a constant which can be any of the above functions to an integer.
 */
int string_to_constant(const char *string, int *output)
{
    bool boolean;
    unsigned modifier;
    unsigned index;
    char buffer[20];
    unsigned length;

    if (string_to_boolean(string, &boolean) == OK) {
        *output = boolean;
        return OK;
    }

    if (string_to_modifier(string, &modifier) == OK) {
        *output = modifier;
        return OK;
    }

    if (string_to_button(string, &index) == OK) {
        *output = index;
        return OK;
    }

    /* translate - to _ */
    for (length = 0; string[length] != '\0'; length++) {
        if (length + 1 >= sizeof(buffer)) {
            return ERROR;
        }

        if (string[length] == '-') {
            buffer[length] = '_';
        } else {
            buffer[length] = string[length];
        }
    }
    length = 0;

    return ERROR;
}
