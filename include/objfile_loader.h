#ifndef __BFLD_OBJECT_FILE_LOADER_H__
#define __BFLD_OBJECT_FILE_LOADER_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "secttypes.h"
#include "symtypes.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


/*
 * Symbol binding types.
 *
 * The binding type determines what to do if multiple symbols declarations
 * with the same name exists.
 *
 * SYMBOL_GLOBAL: The symbol is considered strong and takes presedence.
 * SYMBOL_LOCAL:  The symbol is considered strong and takes presedence.
 * SYMBOL_WEAK:   The symbol is considered weak and can be overridden by
 *                later declarations with the same name.
 *
 * Local means that the symbol is only visible to the specific object file
 * where it is defined, whereas global (and weak) means that the symbol is
 * global across multiple files.
 */
enum symbol_binding
{
    SYMBOL_LOCAL,
    SYMBOL_GLOBAL,
    SYMBOL_WEAK
};


/*
 * Representation of a symbol detected in the object file.
 */
struct objfile_symbol
{
    const char          *name;      // symbol name
    enum symbol_binding binding;    // symbol binding
    enum symbol_type    type;       // symbol type
    bool                common;     // does this symbol refer to the common section?
    bool                relative;   // is the address relative or absolute
    uint64_t            addr;       // absolute or relative address
    uint64_t            align;      // alignment requirements
    uint64_t            offset;     // offset within section
    uint64_t            section;    // section index (0 = no section)

};


/*
 * Metadata bout a a section found in an object file.
 */
struct objfile_section
{
    const char          *name;      // name of the section
    uint64_t            index;      // section index
    size_t              offset;     // offset from file start (if applicable)
    enum section_type   type;       // section type (data, rodata, text, etc.)
    uint64_t            align;      // section alignment requirements
    size_t              size;       // size of the section
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
                      const uint8_t *file_data, 
                      size_t file_size);

    /*
     * Parse section headers and emit section metadata.
     *
     * For each BSS, DATA, RODATA, TEXT, section, the function
     * is expected to invoke the 
     * The return value of the emit_section indicates whether
     * processing should continue (true) or stop (false).
     *
     * If this functions returns anything but 0, it is assumed
     * to mean that an fatal error occurred and parsing is aborted.
     * To indicate that the emit_section callback returned false,
     * the implementation could return ECANCELED by convention.
     */
    int (*parse_sections)(void *objfile_loader_data,
                          bool (*emit_section)(void *callback_data, const struct objfile_section*),
                          void *callback_data);
    
    /* 
     * Parse symbols and emit symbol information.
     *
     * For each symbol, the function is expected to invoke the
     * emit_symbol callback with the symbol name, what binding
     * it has, what type the symbol is and an optional section
     * index and section offset (if the symbol is defined in
     * the current input file).
     *
     * The return value of the emit_symbol callback indicates
     * whether processing should continue (true) or stop (false).
     *
     * If this functions returns anything but 0, it is assumed
     * to mean that an fatal error occurred and parsing is aborted.
     * To indicate that the emit_symbol callback returned false,
     * the implementation could return ECANCELED by convention.
     */
    int (*extract_symbols)(void *objfile_loader_data,
                           bool (*emit_symbol)(void *callback_data, const struct objfile_symbol*),
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
