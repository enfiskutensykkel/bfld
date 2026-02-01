#ifndef BFLD_COMMAND_LINE_OPTIONS_H
#define BFLD_COMMAND_LINE_OPTIONS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>


struct bfld_options
{
    const char *entry;
    const char *output;
    int show_symbols;
    int show_layout;
    int gc_sections;
};


int parse_args(int argc, char **argv, struct bfld_options *opts);


#ifdef __cplusplus
}
#endif
#endif
