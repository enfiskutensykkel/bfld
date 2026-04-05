#ifndef BFLD_ARCHIVE_H
#define BFLD_ARCHIVE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "utils/list.h"
#include "strpool.h"


/* Forward declarations */
struct mfile;
struct objectfile;
struct archive_member;


/*
 * Archive file handle.
 *
 * Archives files (.a files) contain multiple object files that can be 
 * pulled out of the archive and used as input file to the linker. 
 */
struct archive
{
    const char *name;           // name of the archive
    int refcnt;                 // reference counter
    struct mfile *file;         // strong reference to the underlying memory mapped file
    const uint8_t *file_data;   // pointer to file data
    size_t file_size;           // total size of the file
    struct archive_member *members; // dynamic array of archive members
    size_t nmembers;            // number of archive members
    struct strpool names;       // member names
};


/*
 * Archive member file.
 *
 * An archive member file is an object file
 * that can be pulled out of the archive.
 */
struct archive_member
{
    struct archive *archive;    // weak reference to the archive 
    const char* name;           // archive member name (offset in the string pool)
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
 * Take an archive file handle reference.
 */
struct archive * archive_get(struct archive *archive);


/*
 * Release archive file handle reference.
 */
void archive_put(struct archive *archive);


/*
 * Add an archive member file.
 */
struct archive_member * archive_add_member(struct archive *archive,
                                           const char *name,
                                           size_t offset,
                                           size_t size);


/*
 * Look up an archive member file from offset.
 */ 
struct archive_member * archive_get_member(const struct archive *archive, size_t offset);


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
