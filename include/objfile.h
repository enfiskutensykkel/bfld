#ifndef __BFLD_OBJECT_FILE_H__
#define __BFLD_OBJECT_FILE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "archtypes.h"
#include "secttypes.h"
#include "symtypes.h"
#include "mfile.h"
#include "utils/rbtree.h"
#include "utils/list.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/* Forward declaration of objfile_loader */
struct objfile_loader;


/*
 * Representation of an object file.
 * Object files are input files to the linker.
 */
struct objfile
{
    char *name;                 // Filename used when opening the object file
    enum arch_type arch;        // Architecture type
    int refcnt;                 // Reference counter
    mfile *file;                // Reference to the underlying memory map
    const struct objfile_loader *loader;  // Underlying object file loader (front-end)
    void *loader_data;          // Private data for the object file loader
    size_t num_sections;        // Number of sections
    struct rb_tree sections;    // Sections (ordered by section key)
};


/*
 * Represents a section with content, i.e., code or variable data.
 *
 * Sections are always associated with the object file they came from.
 */
struct section
{
    struct rb_node tree_node;   // Section entry in the object file
    struct objfile *objfile;    // Weak reference to the object file where the section came from
    unsigned key;               // Section identifier
    const char *name;           // Section name
    enum section_type type;     // Section type
    size_t size;                // Size of the section content
    uint64_t align;             // Memory alignment requirements
    const uint8_t *content;     // Pointer to section content
    size_t offset;              // Offset into the file to the start of the section
};


/*
 * Represents a relocation that should be applied to a given section.
 *
 * A relocation is a "hole" within in a section that needs to be patched
 * with a resolved address (of a symbol or another section), or the "target".
 */
struct relocation
{
    struct rb_node tree_node;
    struct section *section;    // Weak reference to the section the relocation applies to (the section that must be patched)
    const char *symbol_name;    // Name of the symbol the relocation refers to (or NULL if it refers to a section)
    const struct section *target_section; // Pointer to the section the relocation refers to (or NULL if it refers to a symbol)
    uint64_t offset;            // Offset within section to relocation
    uint32_t type;              // Relocation type (which kind of "patch" to apply)
    int64_t addend;             // Relocation addend
};


/*
 * Is the relocation target a symbol?
 */
#define reloc_target_is_symbol(reloc) ((reloc)->symbol_name != NULL)


/*
 * Is the relocation target a section?
 */
#define reloc_target_is_section(reloc) !reloc_target_is_symbol(reloc)


/*
 * Allocate and initialize a object file handle.
 *
 * This is a low level function which you probably 
 * should not be calling directly.
 *
 * Instead see objfile_load()
 */
int objfile_init(struct objfile **objfile, const struct objfile_loader* loader,
                 const char *name, const uint8_t *data, size_t size);


/*
 * Take an object file reference (increase reference counter).
 */
void objfile_get(struct objfile *objfile);


/*
 * Release an object file reference (decrease reference counter).
 */
void objfile_put(struct objfile *objfile);


/*
 * Attempt load the specified file as an object file.
 *
 * If a loader is specified, this function will try to use that specific
 * loader to load the file. If loader is NULL, registered loaders are attempted
 * one by one until either an appropriate file loader is found or there are 
 * no more loaders to try (and NULL is returned).
 */
struct objfile * objfile_load(mfile *file, const struct objfile_loader *loader);


/* 
 * Symbol reference/definition found in the object file.
 */
struct symbol_info
{
    const char *name;       // symbol name
    bool is_reference;      // is this a symbol reference and not a symbol definition
    bool global;            // is the symbol a global or local symbol?
    bool weak;              // is the symbol weak or strong
    enum symbol_type type;  // symbol type
    bool relative;          // is the offset relative to the section or an absolute address?
    struct section *section;// section the symbol is defined in
    uint64_t offset;        // offset into the section to the definition or absolute address
};


/*
 * Extract symbols from the object file.
 *
 * Parses the underlying object file and invokes 
 * the supplied callback for each symbol in the file.
 *
 * On success, this function returns 0.
 *
 * If the callback returns anything but true, the processing
 * will stop and ECANCELED is returned.
 *
 * Otherwise, if parsing of symbols failed by the underlying
 * object file loader, EBADF is returned.
 */
int objfile_extract_symbols(struct objfile* objfile,
                            bool (*callback)(void *callback_data, struct objfile*, const struct symbol_info*),
                            void *callback_data);

#ifdef __cplusplus
}
#endif
#endif
