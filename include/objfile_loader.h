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
 * Forward declaration of object file.
 */
struct objfile;


/*
 * Forward declaration of object file loader.
 */
struct objfile_loader;


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
 * Operations that an object file loader should support.
 */
struct objfile_ops
{
    /*
     * Determine if the memory mapped file is a format
     * that is supported by the file loader.
     */
    bool (*probe)(const uint8_t *file_data, size_t file_size);

    /*
     * Parse the file data and allocate a input file handle.
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
                            int (*emit_symbol)(void *user_data, const struct objfile_symbol*),
                            void *user_data);
                            

    //int (*load_sections)(struct objfile *file, ...);
    
    //int (*load_relocations)(struct objfile *file, ...);
    

    /*
     * Release the private data handle; we're done with the file.
     */
    void (*release)(void *objfile_loader_data);
};



/*
 * Register an object file loader.
 * Returns the registered loader, or NULL if a loader could not be registered.
 */
extern struct objfile_loader * objfile_loader_register(const char *name, 
                                                       const struct objfile_ops *ops);



/*
 * Try to look up an object file loader by its name.
 */
extern const struct objfile_loader * objfile_loader_find(const char *name);


/*
 * Get the name of an object file loader.
 */
const char * objfile_loader_name(const struct objfile_loader *loader);


/*
 * Go through all registered object file loaders and try to probe the memory area.
 */
extern const struct objfile_loader * objfile_loader_probe(const uint8_t *file_data, 
                                                          size_t file_size);



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
#define OBJFILE_LOADER_INIT __attribute__((constructor))


/*
 * Mark a loader exit function so it is ran on exit.
 *
 * Can be used if your loader needs to allocate and clean
 * up on start up and on exit.
 *
 * Example usage:
 *
 * struct objfile_loader *this_loader = NULL;
 *
 * OBJFILE_LOADER_INIT void my_loader_init(void) {
 *     this_loader = objfile_loader_register("myloader");
 * }
 *
 * OBJFILE_LOADER_EXIT void my_loader_exit(void) {
 *     ...
 *     objfile_loader_unregister(this_loader);
 * }
 */
#define OBJFILE_LOADER_EXIT __attribute__((destructor))


#ifdef __cplusplus
}
#endif
#endif
