#ifndef BFLD_SYMBOLS_H
#define BFLD_SYMBOLS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


// Forward declaration of symbol
struct symbol;


/*
 * Local symbol table.
 * Manages symbols by tracking them by index.
 */
struct symbols
{
    char *name;                 // symbol table name (NOTE: can be NULL)
    int refcnt;                 // reference counter
    size_t capacity;            // size of the array/table
    size_t nsymbols;            // number of symbols in the array
    uint64_t maxidx;            // the highest inserted index
    struct symbol **symbols;    // array/table of symbols (ordered by index)
};


/*
 * Allocate a local symbol table.
 */
struct symbols * symbols_alloc(const char *name);


/*
 * Take a local symbol table reference.
 */
struct symbols * symbols_get(struct symbols *symbols);


/*
 * Release local symbol table reference.
 */
void symbols_put(struct symbols *symbols);


/*
 * Reserve space for at least n symbols in the symbol table.
 *
 * Returns true if the symbol table is able to hold the requested
 * number of symbols.
 *
 * Returns false if the requested number of sections is too large.
 */
bool symbols_reserve(struct symbols *symbols, size_t n);


/*
 * Clear the symbol table.
 */
void symbols_clear(struct symbols *symbols);


/*
 * Insert a symbol in the local symbol table at the specified index.
 *
 * If the specified index is free, a reference to the symbol is taken,
 * the symbol is inserted at the specified index and the function returns 0.
 *
 * If there already is a symbol at the specified index, this function returns
 * EEXIST. 
 *
 * If the optional existing pointer is not NULL and there is already a symbol
 * at the specified index, the pointer is set to the existing symbol.
 * Otherwise the existing pointer is untouched.
 *
 * This function may return ENOMEM if there is not enough space to insert
 * the symbol at the specified index.
 */
int symbols_insert(struct symbols *symbols,
                   uint64_t index,
                   struct symbol *symbol,
                   struct symbol **existing);


/*
 * Release the reference at the specified index.
 */
bool symbols_remove(struct symbols *symbols, uint64_t index);


/*
 * Insert a symbol in the back of the symbol table.
 * Takes a strong reference to the symbol and returns
 * the index the symbol was inserted into.
 *
 * Note: Returns 0 if there was not enough space to insert the symbol.
 */
uint64_t symbols_push(struct symbols *symbols, struct symbol *symbol);


/*
 * Look up symbol at the specified index.
 * Note that this does not take an additional symbol reference.
 */
static inline
struct symbol * symbols_at(const struct symbols *symbols, uint64_t index)
{
    if (index < symbols->capacity) {
        return symbols->symbols[index];
    }
    return NULL;
}


/*
 * Helper function to "pop" the symbol with the highest index off the table.
 * Note that this does NOT release the symbol reference.
 * The caller must call symbol_put() on the returned reference.
 */
struct symbol * symbols_pop(struct symbols *symbols);

#ifdef __cplusplus
}
#endif
#endif
