#include "image.h"
#include "io.h"
#include "inputobj.h"
#include <utils/list.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>


int main(int argc, char **argv)
{
    int c;
    int idx = 0;

    static struct option options[] = {
        {"vm", required_argument, 0, 'i'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    const char *vmpath = DEFAULT_BFVM;

    while ((c = getopt_long_only(argc, argv, ":h", options, &idx)) != -1) {
        switch (c) {
            case 'i':
                vmpath = optarg;
                break;

            case 'h':
                fprintf(stdout, "Usage: %s [--vm objfile] objfile...\n", argv[0]);
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
        fprintf(stderr, "Missing argument objfile\n");
        exit(1);
    }

    int nfiles = 0;
    struct ifile files[argc - optind + 1];

    for (int i = optind; i < argc; ++i) {

        int status = ifile_open(&files[nfiles], vmpath);
        if (status != 0) {
            for (int j = 0; j < nfiles; ++j) {
                ifile_close(&files[j]);
            }
            exit(2);
        }
        ++nfiles;
    }

    struct list_head objfiles = LIST_HEAD_INIT(objfiles);
        
    for (int i = 0; i < nfiles; ++i) {
        fprintf(stderr, "Parsing file: %s\n", argv[optind + i]);

        int status = input_objfile_get_all(&files[i], &objfiles);
        if (status != 0) {
            input_objfile_put_all(&objfiles);
            for (int j = 0; j < nfiles; ++j) {
                ifile_close(&files[j]);
            }
            exit(2);
        }
    }

    input_objfile_put_all(&objfiles);
    for (int j = 0; j < nfiles; ++j) {
        ifile_close(&files[j]);
    }

    exit(0);
}
