#ifndef BFLD_ARCHIVES_H
#define BFLD_ARCHIVES_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils/stringpool.h"

/* Some forward declarations */
struct mfile;
struct objectfile;
struct archive_member;
struct archive_symbol;


/*
 * Archive index.
 *
 * Archive files (.a files) contain multiple object files and a symbol
 * index (called ranlib) that list which symbols are provided by each
 * member file. During symbol resolution, a linker can look at the
 * symbol index and determine if an archive member provides any
 * unresolved symbols. If it does, the member file can be pulled
 * out of the archive and added as an input file to the linker.
 */
struct archive
{
    int refcnt;                         // reference counter
    struct mfile **files;               // dynamic array of file handles
    size_t nfiles;                      // size of the file handle array
    struct archive_member *members;     // dynamic array of archive members
    size_t nmembers;                    // size of the archive member array
    struct archive_symbol *index;       // hash table of symbols (symbol index)
    uint64_t capacity;                  // capacity of the symbol index
    uint64_t entries;                   // number of entries in the symbol index
    uint64_t rehash_threshold;          // rehash threshold
    struct string_pool member_names;    // string pool for archive member names
    struct string_pool symbol_names;    // string pool for symbol names
};


struct archive_member
{
    struct mfile *file;             // weak reference to the memory-mapped file
    uint64_t name;                  // offset in the member_names string pool
    uint64_t offset;                // offset from start of the archive file to member file
    uint64_t size;                  // size of the member file
    struct objectfile *objfile;     // lazily loaded object file handle
};


struct archive_symbol
{
    uint32_t hash;
    uint32_t dfi;
    uint64_t name;
    struct archive_member *member;
};


#ifdef __cplusplus
}
#endif
#endif
