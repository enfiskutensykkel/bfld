#ifndef __ARCHIVE_FILE_H__
#define __ARCHIVE_FILE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "utils/rbtree.h"
#include "mfile.h"
#include "objfile.h"
#include <stddef.h>
#include <stdbool.h>


/*
 * Forward declaration of the archive loader.
 */
struct archive_loader;


/*
 * Representation of an archive file.
 *
 * Archives have a symbol index, allowing the linker
 * to determine if it defines any symbol it needs.
 *
 * Archives contain multiple object files that can
 * be pulled out of the archive and used as input file
 * to the linker, in order to resolve any symbols that
 * the linker needs.
 */
struct archive
{
    char *name;                 // filename used when opening the archive file
    int refcnt;                 // reference counter
    mfile *file;                // reference to the underlying memory-mapped file
    struct rb_tree symbols;     // symbols in the archive symbol index
    struct rb_tree members;     // archive members in the archive

    void *loader_data;          // private data used by the archive loader
    const struct archive_loader *loader;  // archive loader
};


struct archive_member
{
    struct archive *archive;    // pointer to the archive
    struct rb_node tree_node;   // entry in the archive index
    uint64_t member_id;         // generic member identifier (offset, index, ...)
    char *name;                 // name of the archive member (NOTE: may be NULL)
    size_t offset;              // offset into the archive file
    size_t size;                // size of the member
    struct objfile *objfile;    // lazily loaded object file this member refers to
};


struct archive_symbol
{
    struct rb_node tree_node;   // entry in the symbol index
    char *name;                 // name of the symbol
    uint64_t member_id;         // generic member identifier
    struct archive_member *member;  // pointer to the archive where the symbol comes from
};


int archive_init(struct archive **ar, mfile *file, const char *name);


/*
 * Take an archive file reference (increase reference counter).
 */
void archive_get(struct archive *ar);


/*
 * Release an archive file reference (decrease reference counter).
 */
void archive_put(struct archive *ar);


/*
 * Look up an archive member from its identifier.
 */
struct archive_member * archive_lookup_member(const struct archive *ar, uint64_t member_id);


/*
 * Look up a symbol from an archive's symbol index.
 */
struct archive_symbol * archive_lookup_symbol(const struct archive *ar, const char *name);


/*
 * Load the specified archive member as an object file.
 * If the object file is already loaded, the same reference
 * is returned.
 */
struct objfile * archive_load_member_objfile(struct archive_member *member);


/*
 * Attempt to load the specified file as an archive.
 *
 * If loader is specified, this function will try to use that specific
 * archive loader to load the file. If loader is NULL, registered loaders
 * are attempted one by one until either one is found or there are no more
 * loaders to try (and NULL is returned).
 */
struct archive * archive_load(mfile *file, const struct archive_loader *loader);


#ifdef __cplusplus
}
#endif
#endif
