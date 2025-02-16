#include <ctype.h>
#include <string.h>

/* Get the length of @string up to a maximum of @max_length. */
size_t strnlen(const char *string, size_t max_length)
{
    char *null_position;

    null_position = memchr(string, '\0', max_length);
    if (null_position == NULL) {
        return max_length;
    }
    return null_position - string;
}

/* Compare two strings and ignore case. */
int strcasecmp(const char *string1, const char *string2)
{
    int result;

    while (result = tolower(string1[0]) - tolower(string2[0]), result == 0) {
        if (string1[0] == '\0') {
            break;
        }
        string1++;
        string2++;
    }
    return result;
}
