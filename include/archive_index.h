#ifndef BFLD_ARCHIVE_INDEX_H
#define BFLD_ARCHIVE_INDEX_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Some forward declarations */
struct archive;
struct archive_member;


/*
 * Entry in the archive index.
 */
struct archive_entry
{
    uint32_t hash;                  // calculated hash of the symbol
    char *name;                     // symbol name
    struct archive *archive;        // strong reference to the archive that defines the symbol
    struct archive_member *member;  // weak pointer to the archive member where the symbol is defined
};


/*
 * Archive index.
 *
 * When reading archive files (.a files), we track the ranlib index 
 * and map symbols to archive members.
 */
struct archive_index
{
    int refcnt;
    uint64_t capacity;
    uint64_t entries;
    uint64_t threshold;
    struct archive_entry *table;
};


/*
 * Create an archive index.
 */
struct archive_index * archive_index_alloc(void);


/*
 * Take an archive index reference.
 */
struct archive_index * archive_index_get(struct archive_index *index);


/*
 * Release the archive index reference.
 */
void archive_index_put(struct archive_index *index);



bool archive_index_rehash(struct archive_index *index, uint64_t capacity);



/*
 * Add a symbol to the archive index.
 * This creates an entry in the archive index hash table.
 */
bool archive_index_insert(struct archive_index *index,
                          struct archive_member *member,
                          const char *symbol_name);


/*
 * Try to look up the archive member where a symbol is defined.
 */
struct archive_member * archive_index_find(const struct archive_index *index,
                                           const char *symbol_name);


/*
 * Clear an archive index and remove all entries.
 */
void archive_index_clear(struct archive_index *index);


#ifdef __cplusplus
}
#endif
#endif
