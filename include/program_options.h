#ifndef PROGRAM_OPTIONS_H
#define PROGRAM_OPTIONS_H

#include "log.h"

/* Parse the given program arguments.
 *
 * The syntax of the arguments can be:
 * ```
 * -<short option>...
 * --long-option
 * --long-option=
 * --long-option=value
 * --long-option= value
 * --long-option value
 * -<short option>value
 * -<short option> value
 * ```
 *
 * @return ERROR if the program should not continue, OK otherwise.
 */
int parse_program_arguments(int argc, char **argv);

#endif
