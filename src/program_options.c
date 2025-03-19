#include <string.h>

#include "fensterchef.h"
#include "program_options.h"

/* how fensterchef is started */
static const char *program_name;

/* all possible program options */
typedef enum option {
    OPTION_HELP, /* -h, --help */
    OPTION_USAGE, /* --usage */
    OPTION_VERSION, /* -v, --version */
    OPTION_VERBOSITY, /* -d VERBOSITY */
    OPTION_VERBOSE, /* --verbose */
    OPTION_CONFIG, /* --config FILE */
    OPTION_COMMAND, /* -e, --command COMMAND */
} option_t;

/* context the parser needs to parse the options */
static const struct parse_option {
    /* the long option after `--`, may be `NULL` */
    const char *long_option;
    /* a short form of the option after `-` */
    char short_option;
    /* what kind of arguments to expect */
    /* 0 - no arguments */
    /* 1 - single argument */
    int type;
} parse_options[] = {
    [OPTION_HELP] = { "help", 'h', 0 },
    [OPTION_USAGE] = { "usage", '\0', 0 },
    [OPTION_VERSION] = { "version", 'v', 0 },
    [OPTION_VERBOSITY] = { NULL, 'd', 1 },
    [OPTION_VERBOSE] = { "verbose", '\0', 0 },
    [OPTION_CONFIG] = { "config", '\0', 1 },
    [OPTION_COMMAND] = { "command", 'e', 1 },
};

/* Print the usage to standard error output. */
static void print_usage(void)
{
    fprintf(stderr, "Usage: %s [OPTIONS...]\n", program_name);
    fputs("Options:\n\
        -h, --help                  show this help\n\
        -v, --version               show the version\n\
        -d              VERBOSITY   set the logging verbosity\n\
            all                     log everything\n\
            info                    only log informational messages\n\
            error                   only log errors\n\
            nothing                 log nothing\n\
        --verbose                   log everything\n\
        --config        FILE        set the path of the configuration\n\
        -e, --command   COMMAND     run a command within fensterchef\n",
        stderr);

}

/* Print the version to standard error output. */
static void print_version(void)
{
    fputs("fensterchef " FENSTERCHEF_VERSION "\n", stderr);
}

/* Handles given option with given argument @value.
 *
 * @return `ERROR` if the parsing should stop.
 */
static int handle_option(option_t option, char *value)
{
    const char *verbosities[] = {
        [LOG_SEVERITY_ALL] = "all",
        [LOG_SEVERITY_INFO] = "info",
        [LOG_SEVERITY_ERROR] = "error",
        [LOG_SEVERITY_NOTHING] = "nothing",
    };

    switch (option) {
    /* the user wants to receive information */
    case OPTION_HELP:
    case OPTION_USAGE:
        /* nothing to do */
        break;

    /* print the version */
    case OPTION_VERSION:
        print_version();
        return ERROR;

    /* change of logging verbosity */
    case OPTION_VERBOSITY:
        for (log_severity_t i = 0; i < SIZE(verbosities); i++) {
            if (strcasecmp(verbosities[i], value) == 0) {
                log_severity = i;
                return OK;
            }
        }

        fprintf(stderr, "invalid verbosity, pick one of: %s", verbosities[0]);
        for (log_severity_t i = 1; i < SIZE(verbosities); i++) {
            fprintf(stderr, ", %s", verbosities[i]);
        }
        fprintf(stderr, "\n");
        break;

    /* verbose logging */
    case OPTION_VERBOSE:
        log_severity = LOG_SEVERITY_ALL;
        return OK;

    /* set the configuration */
    case OPTION_CONFIG:
        Fensterchef_configuration = value;
        return OK;

    /* run a command */
    case OPTION_COMMAND:
        run_external_command(value);
        return ERROR;
    }

    print_usage();
    return ERROR;
}

/* Parse the given program arguments and load their data into `program_options`.
 */
int parse_program_arguments(int argc, char **argv)
{
    char *argument, *value;
    bool is_long;

    program_name = argv[0];

    argument = argv[1];
    for (int i = 1; i != argc; ) {
        option_t option = 0;

        if (argument[0] != '-') {
            fprintf(stderr, "argument %s is unexpected\n", argument);
            print_usage();
            return ERROR;
        }

        /* check for a long option */
        if (argument[1] == '-') {
            char *equality;

            /* jump over the leading `--` */
            argument += 2;

            /* check for a follong `=` and make it a null terminator */
            equality = strchr(argument, '=');
            if (equality != NULL) {
                equality[0] = '\0';
            }

            /* try to find the long option */
            for (; option < SIZE(parse_options); option++) {
                if (parse_options[option].long_option == NULL) {
                    continue;
                }
                if (strcmp(parse_options[option].long_option,
                            &argument[0]) == 0) {
                    /* either go the end of the argument... */
                    if (equality == NULL) {
                        argument += strlen(argument);
                    /* ...or jump after the equals sign */
                    } else {
                        argument = equality + 1;
                    }
                    break;
                }
            }

            is_long = true;
        } else {
            /* jump over the leading `-` */
            argument++;

            /* try to find the short option */
            for (; option < SIZE(parse_options); option++) {
                if (parse_options[option].short_option == argument[0]) {
                    argument++;
                    break;
                }
            }

            is_long = false;
        }

        /* check if the option exists */
        if (option >= SIZE(parse_options)) {
            fprintf(stderr, "invalid option %s\n", argument);
            print_usage();
            return ERROR;
        }

        value = NULL;

        /* if the option expects no arguments */
        if (parse_options[option].type == 0) {
            /* if the option had the form `--option=value` */
            if (argument[0] != '\0' && is_long) {
                fprintf(stderr, "did not expect argument %s\n", argument);
                print_usage();
                return ERROR;
            }
        } else {
            /* try to take the next argument if the current one has no more */
            if (argument[0] == '\0') {
                i++;
                if (i == argc) {
                    fprintf(stderr, "expected argument\n");
                    print_usage();
                    return ERROR;
                }
                value = argv[i];
            } else {
                value = argument;
                argument += strlen(argument);
            }
        }

        /* go to the next argument */
        if (argument[0] != '\0') {
            argument--;
            argument[0] = '-';
        } else {
            i++;
            if (i < argc) {
                argument = argv[i];
            }
        }

        if (handle_option(option, value) != OK) {
            return ERROR;
        }
    }
    return OK;
}
