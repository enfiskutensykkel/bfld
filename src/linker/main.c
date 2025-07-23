#include "image.h"
#include "mfile.h"
#include "objfile.h"
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

    // Open all input files
    int nfiles = 0;
    mfile *files[argc - optind + 1];

    for (int i = optind; i < argc; ++i) {

        int status = mfile_init(&files[nfiles], argv[i]);

        if (status != 0) {
            for (int j = 0; j < nfiles; ++j) {
                mfile_put(files[j]);
            }
            exit(2);
        }
        ++nfiles;
    }

    // Parse object files and load their data
    struct list_head objfiles = LIST_HEAD_INIT(objfiles);
        
    for (int i = 0; i < nfiles; ++i) {
        fprintf(stderr, "Parsing file: %s\n", argv[optind + i]);

        int status = objfile_load(&objfiles, files[i]);
        if (status != 0) {
            list_for_each_objfile(objfile, &objfiles) {
                objfile_put(objfile);
            }
            for (int j = 0; j < nfiles; ++j) {
                mfile_put(files[j]);
            }
            exit(2);
        }
    }

    for (; nfiles > 0; --nfiles) {
        mfile_put(files[nfiles-1]);
    }
    list_for_each_objfile(objfile, &objfiles) {
        objfile_put(objfile);
    }

    exit(0);
}
