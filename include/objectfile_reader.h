#ifndef BFLD_OBJECT_FILE_READER_FRONTEND_H
#define BFLD_OBJECT_FILE_READER_FRONTEND_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "objectfile.h"
#include "sectiontype.h"
#include "section.h"
#include "sections.h"
#include "symbol.h"
#include "symbols.h"
#include "globals.h"
#include "stringpool.h"


/*
 * Object file reader operations (object file front-end).
 *
 * Object files are input files to the linker, which contain
 * sections with content (data and code).
 *
 * bfld can support different front-ends for loading object files
 * in different formats, i.e., ELF, Mach-O, PE/COFF, etc.
 */
struct objectfile_reader
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
                      struct objectfile *object_file,
                      struct string_pool *string_pool,
                      struct section_table *sections,
                      struct symbol_table *symbols);
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
 * const struct objectfile_reader elf_frontend = {
 *   .name = "elf_frontend",
 *   .probe_file = elf_probe,
 *   .parse_file = elf_parser
 * }
 *
 * __attribute__((constructor)) static void elf_frontend_init(void) {
 *     ...
 *     objectfile_reader_register(&elf_frontend);
 * }
 *
 */
void objectfile_reader_register(const struct objectfile_reader *frontend);


/*
 * Go through all registered object file front-ends and try
 * to probe the memory area to find the front-end that supports
 * this format.
 */
const struct objectfile_reader * 
objectfile_reader_probe(const uint8_t *file_data, size_t file_size, uint32_t *march);


#ifdef __cplusplus
}
#endif
#endif
