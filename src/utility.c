#include <ctype.h>
#include <stdbool.h>
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

/* IMPLEMENTATION of `glob_match()` from the linux source code.
 * I only added '^' in addition to '!'.
 *
 * This is small and simple implementation intended for device blacklists
 * where a string is matched against a number of patterns.  Thus, it
 * does not preprocess the patterns.  It is non-recursive, and run-time
 * is at most quadratic: strlen(@str)*strlen(@pat).
 *
 * An example of the worst case is matching "*aaaaa" against "aaaaaaaaaa".
 * It takes 6 passes over the pattern before matching the string.
 */
bool matches_pattern(char const *pat, char const *str)
{
	/*
	 * Backtrack to previous * on mismatch and retry starting one
	 * character later in the string.  Because * matches all characters
	 * (no exception for /), it can be easily proved that there's
	 * never a need to backtrack multiple levels.
	 */
	char const *back_pat = NULL, *back_str = NULL;

	/*
	 * Loop over each token (character or class) in pat, matching
	 * it against the remaining unmatched tail of str.  Return false
	 * on mismatch, or true after matching the trailing nul bytes.
	 */
	for (;;) {
		unsigned char c = *str++;
		unsigned char d = *pat++;

		switch (d) {
		case '?':	/* Wildcard: anything but nul */
			if (c == '\0')
				return false;
			break;

		case '*':	/* Any-length wildcard */
			if (*pat == '\0')	/* Optimize trailing * case */
				return true;
			back_pat = pat;
			back_str = --str;	/* Allow zero-length match */
			break;

		case '[': {	/* Character class */
			if (c == '\0')	/* No possible match */
				return false;
			bool match = false, inverted = (*pat == '^' || *pat == '!');
			char const *class = pat + inverted;
			unsigned char a = *class++;

			/*
			 * Iterate over each span in the character class.
			 * A span is either a single character a, or a
			 * range a-b.  The first span may begin with ']'.
			 */
			do {
				unsigned char b = a;

				if (a == '\0')	/* Malformed */
					goto literal;

				if (class[0] == '-' && class[1] != ']') {
					b = class[1];

					if (b == '\0')
						goto literal;

					class += 2;
					/* Any special action if a > b? */
				}
				match |= (a <= c && c <= b);
			} while ((a = *class++) != ']');

			if (match == inverted)
				goto backtrack;
			pat = class;
			}
			break;

		case '\\':
			d = *pat++;
		/* fall through */
		default:	/* Literal character */
literal:
			if (c == d) {
				if (d == '\0')
					return true;
				break;
			}
backtrack:
			if (c == '\0' || !back_pat)
				return false;	/* No point continuing */
			/* Try again from last *, one character later in str. */
			pat = back_pat;
			str = ++back_str;
			break;
		}
	}
}
