#ifndef _BFLD_OBJECT_FILE_FRONTEND_H
#define _BFLD_OBJECT_FILE_FRONTEND_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "objfile.h"
#include "section.h"
#include "sections.h"
#include "symbol.h"
#include "symbols.h"
#include "globals.h"


/*
 * Object file front-end operations.
 *
 * Object files are input files to the linker,
 * and contain sections with content (data and code).
 *
 * bfld can support different front-ends for loading object files
 * in different formats, i.e., ELF, Mach-O, PE/COFF, etc.
 */
struct objfile_frontend
{
    /*
     * The name of the object file reader.
     */
    const char *name;

    /*
     * Determine if the memory mapped file content is a format
     * supported by the front-end.
     */
    bool (*probe_file)(const uint8_t *file_data, size_t file_size, uint32_t *march);

    /*
     * Parse file data and load sections and symbols.
     *
     * If this function returns anything but 0, it is assumed
     * to mean that a fatal error occurred and parsing is aborted.
     */
    int (*parse_file)(const uint8_t *file_data, 
                      size_t file_size,
                      struct objfile *objfile,
                      struct sections *sections,
                      struct symbols *symbols,
                      struct globals *globals);
};


/*
 * Register an object file front-end.
 *
 * Example usage:
 *
 * static bool elf_probe(const uint8_t *file_data, size_t file_size)
 * {
 *     ...
 * }
 *
 * static int elf_parser(const uint8_t *data, size_t size, 
 *                       struct objfile *objfile,
 *                       struct sections *sects,
 *                       struct symbols *syms,
 *                       struct globals *globals)
 * {
 *     ...
 * }
 *
 * const struct objfile_frontend elf_frontend = {
 *   .name = "elf_frontend",
 *   .probe_file = elf_probe,
 *   .parse_file = elf_parser
 * }
 *
 * __attribute__((constructor)) static void elf_frontend_init(void) {
 *     ...
 *     objfile_frontend_register(&elf_frontend);
 * }
 *
 */
void objfile_frontend_register(const struct objfile_frontend *frontend);


/*
 * Go through all registered object file front-ends and try
 * to probe the memory area to find the front-end that supports
 * this format.
 */
const struct objfile_frontend * objfile_frontend_probe(const uint8_t *file_data, size_t file_size, uint32_t *march);


#ifdef __cplusplus
}
#endif
#endif
