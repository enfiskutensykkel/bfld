#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <elf.h>
#include <getopt.h>


struct option options[] = {
    {"interpreter", required_argument, 0, 'i'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};


int main(int argc, char **argv)
{
    int c;
    int idx = 0;

    const char *interpreter = DEFAULT_INTERPRETER;

    while ((c = getopt_long_only(argc, argv, ":h", options, &idx)) != -1) {
        switch (c) {
            case 'i':
                interpreter = optarg;
                break;

            case 'h':
                fprintf(stderr, "Usage: %s [--interpreter INTERPRETER] FILE [FILE...]\n", argv[0]);
                exit(0);

            case ':':
                fprintf(stderr, "Missing value for option %s\n", argv[optind-1]);
                exit(1);

            default:
                fprintf(stderr, "Unknown option %s\n", argv[optind-1]);
                exit(1);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Missing argument FILE\n");
        exit(1);
    }

    fprintf(stderr, "Using interpreter %s\n", interpreter);

    for (int i = optind; i < argc; ++i) {
        fprintf(stderr, "Parsing file: %s\n", argv[i]);
    }

    exit(0);
}
