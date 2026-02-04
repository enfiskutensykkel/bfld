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
struct archive;
struct archive_member;


// struct member_reference
// struct symbol_reference


/*
 * Entry in the archive index.
 */
struct archive_entry
{
    // FIXME: 32-bit is sufficient?
    uint64_t hash;                  // calculated hash of the symbol
    uint64_t name;                  // symbol name
    // TODO: separate hash table for archive members, use index to refer it (fewer gets/puts)
    struct archive *archive;        // strong reference to the archive that defines the symbol
    struct archive_member *member;  // weak pointer to the archive member where the symbol is defined
};


/*
 * Archive index.
 *
 * When reading archive files (.a files), we track the ranlib index 
 * and map symbols to archive members.
 */
struct archives
{
    int refcnt;
    uint64_t capacity;
    uint64_t entries;
    uint64_t threshold;
    struct archive_entry *table;
    struct string_pool stringpool;
};


/*
 * Create an archive index.
 */
struct archives * archives_alloc(void);


/*
 * Take an archive index reference.
 */
struct archives * archives_get(struct archives *index);


/*
 * Release the archive index reference.
 */
void archives_put(struct archives *index);


/*
 * Add a symbol to the archive index.
 * This creates an entry in the archive index hash table.
 */
bool archives_insert_symbol(struct archives *index,
                            struct archive_member *member,
                            const char *symbol_name);


/*
 * Try to look up the archive member where a symbol is defined.
 */
struct archive_member * archives_find_symbol(const struct archives *index,
                                             const char *symbol_name);


/*
 * Clear an archive index and remove all symbol entries.
 */
void archives_clear_symbols(struct archives *index);


#ifdef __cplusplus
}
#endif
#endif
