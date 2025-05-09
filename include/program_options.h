#ifndef PROGRAM_OPTIONS_H
#define PROGRAM_OPTIONS_H

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
 * This function exits if the user requested it via e.g. `--help` or if an error
 * occured.
 */
void parse_program_arguments(int argc, char **argv);

#endif
