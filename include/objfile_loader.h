#ifndef __BFLD_OBJECT_FILE_LOADER_H__
#define __BFLD_OBJECT_FILE_LOADER_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "mfile.h"
#include "symtypes.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


/*
 * Forward declaration of object file.
 */
struct objfile;


/*
 * Private data for the object file loader.
 *
 * Each file loader implements their own version of this struct,
 * allowing them to keep necessary private data, for example,
 * pointers to headers, start of sections, symbols, etc.
 */
struct objfile_private;


/*
 * Forward declaration of object file loader.
 */
struct objfile_loader;


/*
 * Operations that a file loader should support.
 */
struct objfile_loader_ops
{
    /*
     * Determine if the memory mapped file is a format
     * that is supported by the file loader.
     */
    bool (*probe)(const uint8_t *file_data, size_t file_size);

    /*
     * Parse the file data and allocate a input file handle.
     */
    int (*parse_file)(struct objfile_private **objfile_data, 
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
    int (*extract_symbols)(struct objfile_private *objfile_data,
                           int (*emit_symbol)(const char *name, 
                                              enum symbol_binding,
                                              enum symbol_type,
                                              uint64_t sect_idx,
                                              size_t sect_offset));
    


    //int (*load_sections)(struct objfile *file, ...);
    
    //int (*load_relocations)(struct objfile *file, ...);
    

    /*
     * Release the private data handle; we're done with the file.
     */
    void (*release)(struct objfile_private *objfile_data);
};



/*
 * Register an object file loader.
 * Returns the registered loader, or NULL if a loader could not be registered.
 */
extern struct objfile_loader * objfile_loader_register(const char *name, 
                                                       const struct objfile_loader_ops *ops);


/*
 * Go through all registered object file loaders and try to probe the file.
 *
 * Returns an object file handle if we found a loader and were able
 * to probe the file.
 */
extern struct objfile * objfile_loader_probe_all(mfile *mfile);


/*
 * Mark a loader initializer so that it is invoked on startup.
 *
 * Example usage:
 *
 * const struct objfile_loader_ops my_ops = {
 *   .probe = my_elf_probe,
 *   .parse_file = &my_elf_parser,
 *   ...
 * }
 *
 * OBJFILE_LOADER_INIT void my_elf_loader_init(void) {
 *     ...
 *     objfile_loader_register("my_elf_loader", &my_ops);
 * }
 */
#define OBJFILE_LOADER_INIT __attribute__((constructor)


#ifdef __cplusplus
}
#endif
#endif
