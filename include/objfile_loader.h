#ifndef __BFLD_OBJECT_FILE_LOADER_H__
#define __BFLD_OBJECT_FILE_LOADER_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "symtypes.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


/*
 * Representation of a symbol detected in the object file.
 */
struct objfile_symbol
{
    const char          *name;      // symbol name
    uint64_t            value;      // value of the symbol
    enum symbol_binding bind;       // symbol binding
    enum symbol_type    type;       // symbol type
    bool                defined;    // is the symbol defined?
    uint64_t            sect_idx;   // section index (if defined)
    size_t              size;       // size of the defined symbol
    size_t              offset;     // offset within the section (if defined)
};


/*
 * Represents an object file loader.
 *
 * bfld supports different front-ends for loading object files in
 * different formats (i.e., ELF, Mach-O, etc).
 *
 * This struct should be declared statically by a loader, with
 * members set. It contains the operations supported by the loader.
 */
struct objfile_loader
{
    /*
     * The name of the object file loader.
     */
    const char *name;

    /*
     * Determine if the memory mapped file is a format
     * that is supported by the file loader.
     */
    bool (*probe)(const uint8_t *file_data, size_t file_size);

    /*
     * Parse the file data and allocate a private file data (if needed).
     *
     * This function should set up necessary pointers and handles
     * to avoid parsing the file later.
     *
     * If this function returns anything but 0, it is assumed
     * to mean that an fatal error occurred and parsing is aborted.
     */
    int (*parse_file)(void **objfile_loader_data, 
                      const char *filename,
                      const uint8_t *file_data, 
                      size_t file_size);
    
    /* 
     * Extract symbol declarations from the input file.
     *
     * For each symbol, the function is expected to invoke the
     * emit_symbol callback with the symbol name, what binding
     * it has, what type the symbol is and an optional section
     * index and section offset (if the symbol is defined in
     * the current input file).
     *
     * If symbol type is other than SYMBOL_UNDEFINED, the symbol
     * is assumed to be defined in the specified section (given by
     * sect_idx) and offset. If symbol type is SYMBOL_UNDEFINED,
     * sect_idx and sect_offset are ignored.
     *
     * The return value of the emit_symbol callback indicates
     * the following:
     *  0 - the symbol was added to the symbol table, 
     *      continue parsing
     * >0 - the symbol was rejected, but this was handled,
     *      continue parsing
     * <0 - fatal error, stop parsing
     */
    void (*extract_symbols)(void *objfile_loader_data,
                            int (*emit_symbol)(void *callback_data, const struct objfile_symbol*),
                            void *callback_data);
                            

    //int (*load_sections)(struct objfile *file, ...);
    
    //int (*load_relocations)(struct objfile *file, ...);
    
    /*
     * Release the private data associated with the file; we're done with the file.
     */
    void (*release)(void *objfile_loader_data);
};



/*
 * Register an object file loader.
 */
int objfile_loader_register(const struct objfile_loader *loader);



/*
 * Try to look up an object file loader by its name.
 */
const struct objfile_loader * objfile_loader_find(const char *name);


/*
 * Go through all registered object file loaders and 
 * try to probe the memory area to check if the loader
 * supports this format.
 */
const struct objfile_loader * objfile_loader_probe(const uint8_t *file_data, size_t file_size);


/*
 * Mark a loader initializer so that it is invoked on startup.
 *
 * Example usage:
 *
 * const struct objfile_loader my_loader = {
 *   .name = "my_elf_loader",
 *   .probe = my_elf_probe,
 *   .parse_file = &my_elf_parser,
 *   ...
 * }
 *
 * OBJFILE_LOADER_INIT void my_elf_loader_init(void) {
 *     ...
 *     objfile_loader_register(&my_loader);
 * }
 */
#define OBJFILE_LOADER_INIT __attribute__((constructor))


#ifdef __cplusplus
}
#endif
#endif
