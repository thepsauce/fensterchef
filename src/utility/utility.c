#include <ctype.h>
#include <errno.h> // EINTR
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h> // waitpid()
#include <unistd.h> // fork(), setsid(), _exit(), execl()

#include "utility/utility.h"

/* Run @command within a shell in the background. */
void run_shell(const char *command)
{
    pid_t child_process_id;

    /* using `fork()` twice and `_exit()` will give the child process to the
     * init system so we do not need to worry about cleaning up dead child
     * processes; we want to run the shell in a new session
     * TODO: explain why `setsid()` is needed
     *
     * `_exit()` is used for child processes
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
                _exit(EXIT_FAILURE);
            }
            /* this code is executed in the grandchild process */
            (void) execl("/bin/sh", "sh", "-c", command, (char*) NULL);
            /* this point is only reached if `execl()` failed */
            _exit(EXIT_FAILURE);
        } else {
            /* exit the child process */
            _exit(EXIT_SUCCESS);
        }
    } else {
        /* wait until the child process exits */
        (void) waitpid(child_process_id, NULL, 0);
    }
}

/* Run @command as command within a shell and get the first line from it. */
char *run_shell_and_get_output(const char *command)
{
    pid_t child_process_id;
    int pipe_descriptors[2];
    char buffer[1024];
    ssize_t count;

    /* open a pipe */
    if (pipe(pipe_descriptors) < 0) {
        return NULL;
    }

    child_process_id = fork();
    switch (child_process_id) {
    /* fork failed */
    case -1:
        close(pipe_descriptors[0]);
        close(pipe_descriptors[1]);
        return NULL;

    /* child process */
    case 0:
        close(pipe_descriptors[0]);
        if (pipe_descriptors[1] != STDOUT_FILENO) {
            dup2(pipe_descriptors[1], STDOUT_FILENO);
            close(pipe_descriptors[1]);
        }
        (void) execl("/bin/sh", "sh", "-c", command, (char*) NULL);
        _exit(EXIT_FAILURE);
        break;
    }

    /* parent process */
    close(pipe_descriptors[1]);

    count = read(pipe_descriptors[0], buffer, sizeof(buffer) - 1);

    close(pipe_descriptors[0]);

    char *const new_line = memchr(buffer, '\n', count);
    if (new_line != NULL) {
        count = new_line - buffer;
    }

    return xmemdup(buffer, count);
}

/* Check if a character is a line ending character. */
int islineend(int character)
{
    return character == '\n' || character == '\v' ||
        character == '\f' || character == '\r';
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
        case '?':   /* Wildcard: anything but nul */
            if (c == '\0')
                return false;
            break;

        case '*':   /* Any-length wildcard */
            if (*pat == '\0')   /* Optimize trailing * case */
                return true;
            back_pat = pat;
            back_str = --str;   /* Allow zero-length match */
            break;

        case '[': { /* Character class */
            if (c == '\0')  /* No possible match */
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

                if (a == '\0')  /* Malformed */
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
        default:    /* Literal character */
literal:
            if (c == d) {
                if (d == '\0')
                    return true;
                break;
            }
backtrack:
            if (c == '\0' || !back_pat)
                return false;   /* No point continuing */
            /* Try again from last *, one character later in str. */
            pat = back_pat;
            str = ++back_str;
            break;
        }
    }
}

/* Use the FNV-1 hash algorithm to compute the hash of @name. */
uint32_t get_fnv1_hash(const char *name)
{
    const uint32_t fnv_prime = 0x01000193;
    const uint32_t fnv_offset_basis = 0x811c9dc5;

    uint32_t hash = 0;

    hash = fnv_offset_basis;
    for (; name[0] != '\0'; name++) {
        hash *= fnv_prime;
        hash ^= name[0];
    }
    return hash;
}
