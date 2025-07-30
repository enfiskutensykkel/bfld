#ifndef __BFLD_OBJECT_FILE_H__
#define __BFLD_OBJECT_FILE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "secttypes.h"
#include "symtypes.h"
#include "mfile.h"
#include "utils/rbtree.h"
#include "utils/list.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/*
 * Representation of an object file.
 * Object files are input files to the linker.
 */
struct objfile
{
    char *name;             // filename used when opening the object file
    int refcnt;             // reference counter
    mfile *file;            // reference to the underlying memory-map
    void *loader_data;      // private data for the object file loader
    const struct objfile_loader *loader;  // the underlying object file loader (front-end)
    size_t num_sections;    // number of sections
    struct rb_tree sections;// map of sections
};


/* Forward declaration of a merged section mapping */
struct section_mapping;


/*
 * Representation of a section with content.
 * Sections are associated with the object file they came from.
 */
struct section
{
    struct section_mapping *merge_mapping; // Merged section this section is part of (NULL until merged)
    struct rb_node tree_node;
    struct objfile *objfile;// Reference to the object file where the section came from
    uint64_t key;           // Section identifier
    char *name;             // Section name (can be NULL)
    enum section_type type; // Section type
    size_t size;            // Size of the section content
    uint64_t align;         // Memory alignment requirements
    const uint8_t *content; // Pointer to section content
    size_t offset;          // Offset into the file to the start of the section, used internally only
};


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
struct syminfo
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
 * Convenience type for symbol callback.
 */
typedef bool (*objfile_syminfo_cb)(void *callback_data, 
                                   struct objfile*,
                                   const struct syminfo*);


/*
 * Extract all symbols from the object file.
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
                            objfile_syminfo_cb,
                            void *callback_data);



#ifdef __cplusplus
}
#endif
#endif
