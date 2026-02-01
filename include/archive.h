#ifndef BFLD_ARCHIVE_FILE_H
#define BFLD_ARCHIVE_FILE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "utils/rbtree.h"

/* Forward declarations */
struct mfile;
struct objectfile;
struct archive_reader;


/*
 * Archive file handle.
 *
 * Archives have a symbol index, allowing the linker to determine
 * if it defines any symbol that it needs.
 *
 * Archives contain multiple object files that can be pulled
 * out of the archive and used as input file to the linker, in 
 * order to resolve any symbols that the linker needs.
 */
struct archive
{
    char *name;                 // name of the archive
    struct mfile *file;         // strong reference to the underlying memory mapped file
    int refcnt;                 // reference counter
    struct rb_tree members;     // the archive's member files (map ordered by offsets)
    size_t nmembers;            // number of archive members
    const uint8_t *file_data;   // pointer to file data
    size_t file_size;           // total size of the file
};


/*
 * Archive member file.
 * An archive member file is an object file
 * that can be pulled out of the archive.
 */
struct archive_member
{
    struct archive *archive;    // weak reference to the archive 
    char *name;                 // name/identifier of the archive member (NOTE: may be NULL)
    struct rb_node map_entry;   // map entry
    size_t offset;              // offset to the member file
    size_t size;                // size of the member file
    const uint8_t *content;     // pointer to member content
    struct objectfile *objfile; // lazily loaded object file reference
};


struct archive * archive_alloc(struct mfile *file,
                               const char *name,
                               const uint8_t *file_data,
                               size_t file_size);


/*
 * Add an archive member file.
 */
struct archive_member * archive_add_member(struct archive *archive,
                                           const char *name,
                                           size_t offset,
                                           size_t size);


/*
 * Look up an archive member by offset.
 */
struct archive_member * archive_get_member(const struct archive *archive,
                                           size_t offset);


/*
 * Take an archive file handle reference.
 */
struct archive * archive_get(struct archive *archive);


/*
 * Release archive file handle reference.
 */
void archive_put(struct archive *archive);


/*
 * Extract the specified archive member as an object file.
 *
 * If the object file is already loaded, the reference counter is 
 * increased and the same reference is returned.
 *
 * Note that this takes an object file handle reference,
 * so the caller must call objectfile_put() to relase it.
 */
struct objectfile * archive_extract_member(struct archive_member *member);


/*
 * Helper function to determine if an archive member was already extracted.
 */
static inline
bool archive_is_member_extracted(struct archive_member *member)
{
    return member->objfile != NULL;
}


#ifdef __cplusplus
}
#endif
#endif
