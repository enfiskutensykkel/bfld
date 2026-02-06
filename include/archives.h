#ifndef BFLD_ARCHIVES_H
#define BFLD_ARCHIVES_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "stringpool.h"

/* Some forward declarations */
struct archive;
struct archive_member;
struct archive_symbol;


/*
 * Archive index.
 *
 * This structure provides a global index over symbols and which
 * archive files provide them.
 *
 * Archive files (.a files) contain multiple object files and a symbol
 * index (called ranlib) that list which symbols are provided by each
 * member file. During symbol resolution, a linker can look at the
 * symbol index and determine if an archive member provides any
 * unresolved symbols. If it does, the member file can be pulled
 * out of the archive and added as an input file to the linker.
 */
struct archives
{
    int refcnt;                     // reference counter
    struct archive **archives;      // dynamic array of archives (sorted by pointer value)
    uint64_t narchives;             // number of archives
    struct archive_symbol *index;   // hash table of symbols (symbol index)
    uint64_t capacity;              // capacity of the symbol index
    uint64_t entries;               // entries in the symbol index
    uint64_t rehash_threshold;      // rehash threshold for the symbol index
    struct string_pool names;       // string pool for symbol names
};


/*
 * Entry in the archive symbol index.
 */
struct archive_symbol
{
    uint32_t hash;                  // calculated hash of the symbol
    uint32_t dfi;                   // distance from ideal
    uint64_t name;                  // symbol name (offset into the string pool)
    struct archive_member *member;  // weak pointer to the archive member where the symbol is defined
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
