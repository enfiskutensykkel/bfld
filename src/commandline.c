#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <logging.h>
#include "commandline.h"


// strnlen is a POSIX extension
extern size_t strnlen(const char *s, size_t maxlen);


char * format_option(char *buf, 
                     size_t bufsz, 
                     int has_arg, 
                     const char *optname, 
                     const char *argname)
{
    size_t i = 0;

    if (bufsz == 0) {
        return buf + i;
    }

    for (size_t j = 0; i < bufsz && optname[j] != '\0'; ++i, ++j) {
        buf[i] = optname[j];
    }

    if (i == bufsz) {
        return buf + i;
    }

    if (has_arg == no_argument) {
        return buf + i;
    }
    assert(argname != NULL);

    if (has_arg == optional_argument) {
        buf[i++] = '[';
        if (i == bufsz) {
            return buf + i;
        }
    }

    buf[i++] = has_arg == optional_argument ? '=' : ' ';
    if (i == bufsz) {
        return buf + i;
    }

    for (size_t j = 0; i < bufsz && argname[j] != '\0'; ++i, ++j) {
        buf[i] = argname[j];
    }
    if (i == bufsz) {
        return buf + i;
    }

    if (has_arg == optional_argument) {
        buf[i++] = ']';
        if (i == bufsz) {
            return buf + i;
        }
    }

    return buf + i;
}


static const char * next_word(const char *s)
{
    while (*s != '\0' && *s < '!') {
        ++s;
    }

    return *s != '\0' ? s : NULL;
}


static int wordlen(const char *s)
{
    int n = 0;

    while (s[n] != '\0' && s[n] >= '!') {
        ++n;
    }

    return n;
}


static void print_option(FILE *fp,
                         const char *shortname,
                         const char *longname,
                         int has_arg,
                         const char *argname,
                         const char *help)
{
    int col = 32;
    char buffer[128];
    memset(buffer, ' ', 2);
    char *s = &buffer[0] + 2;

    if (argname == NULL) {
        has_arg = no_argument;
    }

    if (shortname) {
        s = format_option(s, sizeof(buffer) - (s - &buffer[0]),
                          has_arg, shortname, argname);

        if (longname) {
            s += snprintf(s, sizeof(buffer) - (s - buffer), ", ");
        }
    }

    if (longname) {
        s = format_option(s, sizeof(buffer) - (s - buffer),
                has_arg, longname, argname);
    }

    if (sizeof(buffer) - (s - buffer) > 0) {
        *s = '\0';
    }

    int n = strnlen(buffer, sizeof(buffer));
    fprintf(fp, "%.*s", n, buffer);

    memset(buffer, ' ', col);
    buffer[col] = '\0';

    if (n < col) {
        fprintf(fp, "%.*s", col - n, buffer);
    } else {
        fprintf(fp, "\n%s", buffer);
    }
    int curr = col;

    const char *word = next_word(help);

    while (word != NULL) {
        n = wordlen(word);
        
        if (curr + n > 79) {
            fprintf(fp, "\n%s", buffer);
            curr = col;
        }
        curr += n + 1;
        fprintf(fp, " %.*s", n, word);
        word = next_word(word + n);
    }
    fprintf(fp, "\n");
}


int parse_args(int argc, char **argv, struct bfld_options *opts)
{
    int c;
    int idx = -1;

    struct option options[] = {
        {"verbose", optional_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"output", required_argument, 0, 'o'},
        {"entry", required_argument, 0, 'e'},
        {"show-symbols", no_argument, &opts->show_symbols, 1},
        {"show-layout", no_argument, &opts->show_layout, 1},
        {"gc-sections", no_argument, &opts->gc_sections, 1},
        {"no-gc-sections", no_argument, &opts->gc_sections, 0},
        {0, 0, 0, 0}
    };

    while ((c = getopt_long_only(argc, argv, ":hvo:e:", options, &idx)) != -1) {
        switch (c) {

            case 0:
                break;

            case 'o':
                opts->output = optarg;
                break;

            case 'e':
                opts->entry = optarg;
                break;

            case 'v':
                if (optarg == NULL) {
                    ++log_level;
                } else {
                    char *endptr = NULL;
                    int verbosity = strtol(optarg, &endptr, 10);
                    if (*endptr != '\0') {
                        log_error("Invalid log level: '%s'", optarg);
                        return -1;
                    }
                    log_level = verbosity;
                }
                break;

            case 'h':
                fprintf(stdout, "Usage: %s [OPTIONS] FILE...\n", argv[0]);
                fprintf(stdout, "Options:\n");
                print_option(stdout, "-h", "--help", no_argument, NULL, "Show this help and quit.");
                print_option(stdout, "-v", "--verbose", optional_argument, "level", "Increase log level.");
                print_option(stdout, "-o", "--output", required_argument, "FILE", "Set output file name.");
                print_option(stdout, "-e", "--entry", required_argument, "ADDRESS", "Set start address.");
                print_option(stdout, "--[no-]gc-sections", NULL, no_argument, NULL, "Enable or disable garbage collection of dead code (default is to garbage collect).");
                print_option(stdout, "--show-symbols", NULL, no_argument, NULL, "Print global symbol table.");
                print_option(stdout, "--show-layout", NULL, no_argument, NULL, "Print image layout information.");
                return 0;

            case ':':
                log_error("Missing value for option '%s'", argv[optind-1]);
                return -1;

            default:
                log_error("Unrecognized option '%s'", argv[optind-1]);
                return -1;
        }
    }

    return optind;
}
