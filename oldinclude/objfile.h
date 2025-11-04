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

/* Forward declaration of section */
struct section;


/*
 * Representation of an object file.
 * Object files are input files to the linker, and contain sections
 * with content (data and code). 
 */
struct objfile
{
    char *name;              // Filename used when opening the object file
    enum arch_type arch;     // Architecture type
    int refcnt;              // Reference counter
    mfile *file;             // Reference to the underlying memory map
    const struct objfile_loader *loader;  // Underlying object file loader (front-end)
    void *loader_data;       // Private data for the object file loader
    size_t num_sections;     // Number of sections
    struct rb_tree sections; // Map of sections, ordered by sect_key
};


///*
// * Represents a section (of an object file) with content in it,
// * i.e., code or variables/data, that needs to go into the final image.
// */
//struct section
//{
//    struct rb_node tree_node;
//    struct objfile *objfile;// Reference to the object file where the section came from (weak reference)
//    uint64_t sect_key;      // Section identifier
//    char *name;             // Section name
//    enum section_type type; // Section type
//    size_t size;            // Size of the section content
//    uint64_t align;         // Memory alignment requirements
//    const uint8_t *content; // Pointer to section content
//};


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
 * Get the specified section from the object file.
 *
 * Note that this takes a section reference (reference count),
 * and you should call section_put() to release it.
 */
struct section * objfile_get_section(struct objfile *objfile, 
                                     uint64_t sect_key);


/*
 * Get the specified section from the object file.
 *
 * Note that this takes a section reference (reference count),
 * and you should call section_put() to release it.
 */
struct section * objfile_get_section_by_name(struct objfile *objfile, 
                                             const char *name);


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
    uint64_t sect_key;      // section the symbol is defined in (or 0 if no definition)
    uint64_t offset;        // offset into the section to the definition or absolute address (or 0 if no definition)
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


///*
// * Relocation found in in an object file.
// *
// * A relocation is a "hole" within in a section that needs to be patched
// * with a resolved address (of a symbol or another section), or the "target".
// */
//struct reloc_info
//{
//    uint64_t source_sect_key;   // Section where the relocation should be applied
//    const char *symbol_name;    // Name of the symbol the relocation refers to (or NULL if it refers to a section)
//    uint64_t target_sect_key;    // Pointer to the section the relocation refers to (or 0 if it refers to a symbol)
//    uint64_t offset;            // Offset within section to relocation
//    uint32_t type;              // Relocation type (which kind of "patch" to apply)
//    int64_t addend;             // Relocation addend
//};


/*
 * Extract relocations from the object file for a given section.
 *
 * Parses the underlying object file and invokes 
 * the supplied callback for each symbol in the file
 * that should be applied to the given section.
 *
 * On success, this function returns 0.
 *
 * If the callback returns anything but true, the processing
 * will stop and ECANCELED is returned.
 *
 * Otherwise, if parsing of symbols failed by the underlying
 * object file loader, EBADF is returned.
 */
int objfile_extract_relocs(struct objfile *objfile,
                           //uint64_t sect_key,
                           bool (*callback)(void *callback_data,
                                            struct objfile*,
                                            const struct reloc_info*),
                           void *callback_data);

#ifdef __cplusplus
}
#endif
#endif
