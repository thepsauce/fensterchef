#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h> // waitpid()
#include <unistd.h> // fork(), setsid(), _exit(), execl()

#include "utility.h"

/* Run @command within a shell in the background. */
void run_shell(const char *command)
{
    int child_process_id;

    /* using `fork()` twice and `_exit()` will give the child process to the
     * init system so we do not need to worry about cleaning up dead child
     * processes; we want to run the shell in a new session
     * TODO: explain why `setsid()` is needed
     */

    /* create a child process */
    child_process_id = fork();
    if (child_process_id == 0) {
        /* this code is executed in the child */

        /* create a grandchild process */
        if (fork() == 0) {
            /* make a new session */
            if (setsid() == -1) {
                /* TODO: when does this happen? */
                exit(EXIT_FAILURE);
            }
            /* this code is executed in the grandchild process */
            (void) execl("/bin/sh", "sh", "-c", command, (char*) NULL);
            /* this point is only reached if `execl()` failed */
            exit(EXIT_FAILURE);
        } else {
            /* exit the child process */
            _exit(0);
        }
    } else {
        /* wait until the child process exits */
        (void) waitpid(child_process_id, NULL, 0);
    }
}

/* Run @command as new process and get the first line from it. */
char *run_command_and_get_output(const char *command)
{
    FILE *process;
    char *line;
    size_t length, capacity;

    process = popen(command, "r");
    if (process == NULL) {
        return NULL;
    }

    capacity = 128;
    line = xmalloc(capacity);
    length = 0;
    /* read all output from the process, stopping at the end (EOF) or a new line
     */
    for (int c; (c = fgetc(process)) != EOF && c != '\n'; ) {
        if (length + 1 == capacity) {
            capacity *= 2;
            RESIZE(line, capacity);
        }
        line[length++] = c;
    }
    line[length] = '\0';
    pclose(process);
    return line;
}

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
