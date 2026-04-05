#ifndef BFLD_SYMBOLS_H
#define BFLD_SYMBOLS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "symbol.h"
#include "utils/deque.h"
#include "utils/table.h"


/*
 * Symbols worklist.
 * Used to process symbols in order.
 */
struct symbols
{
    int refcnt;         // 0 if embedded/stack allocated, >0 if shared
    struct deque q;     // internal queue structure
    uint64_t nsymbols;  // number of symbols in the queue
};


/*
 * Local symbols table.
 * Used for looking up symbols by index rather than name.
 * Must be initialized to zero before use.
 */
struct symbol_table
{
    struct table tbl;   // internal table structure
    uint64_t capacity;  // table capacity
    uint64_t nsymbols;  // number of symbols in the table
};


/*
 * Allocate a reference counted symbol queue and reserve
 * space for at least n symbols.
 */
struct symbols * symbols_alloc(uint64_t n);


/*
 * Take a symbol queue reference.
 */
struct symbols * symbols_get(struct symbols *symq);


/*
 * Release a symbols queue reference.
 */
void symbols_put(struct symbols *symq);


/*
 * Reserve space for at least n symbols in the symbol queue
 *
 * Returns true if the symbol queue is able to hold the requested
 * number of symbols.
 *
 * Returns false if the requested number of sections is too large.
 */
static inline
bool symbols_reserve(struct symbols *symq, size_t n)
{
    return deque_reserve(&symq->q, n);
}


/*
 * Insert a symbol at the back of the symbol queue.
 * Note that this takes a strong reference to the symbol.
 * Returns true if the symbol was inserted, or false if insertion failed.
 */
static inline 
bool symbols_push(struct symbols *symq, struct symbol *sym)
{
    if (!deque_push_back(&symq->q, symbol_get(sym))) {
        symbol_put(sym);
        return false;
    }
    symq->nsymbols = symq->q.size;
    return true;
}


/*
 * Peek at the symbol at the given position relative to
 * the start of the queue.
 */
static inline
struct symbol * symbols_at(const struct symbols *symq, uint64_t position)
{
    return (struct symbol*) deque_peek(&symq->q, position);
}


static inline
struct symbol * symbols_peek(const struct symbols *symq)
{
    return symbols_at(symq, 0);
}


/*
 * Remove the first symbol in the queue and return it.
 *
 * Note that this does NOT release the symbol reference. Ownership is
 * transferred to the caller.
 *
 * The caller must call symbol_put() on the returned reference.
 * Returns NULL if the queue is empty.
 */
static inline
struct symbol * symbols_pop(struct symbols *symq)
{
   struct symbol *sym = (struct symbol*) deque_pop_front(&symq->q);
   symq->nsymbols = symq->q.size;
   return sym;
}


/*
 * Clear the symbol queue.
 * Releaseas all symbols remaining in the queue.
 */
static inline
void symbols_clear(struct symbols *symq)
{
    struct symbol *sym;

    while ((sym = symbols_pop(symq)) != NULL) {
        symbol_put(sym);
    }
    deque_clear(&symq->q);
}


/*
 * Is the symbol queue empty?
 */
static inline
bool symbols_empty(const struct symbols *symq)
{
    return symq->nsymbols == 0;
}


/*
 * Get the number of symbols in the queue.
 */
static inline
uint64_t symbols_size(const struct symbols *symq)
{
    return symq->nsymbols;
}


/*
 * Reserve space for at least n symbols in the symbol table.
 * Returns true if the symbol table can hold the required number
 * of symbols.
 * Returns false if the requested number of symbols is too large.
 */
static inline
bool symbol_table_reserve(struct symbol_table *symtab, uint64_t n)
{
    return table_reserve(&symtab->tbl, n);
}


/*
 * Get the symbol at the specified index.
 */
static inline
struct symbol * symbol_table_at(const struct symbol_table *symtab, uint64_t idx)
{
    return (struct symbol*) table_at(&symtab->tbl, idx);
}


/*
 * Insert a symbol at the specified index.
 *
 * Returns true if the symbol was inserted at the specified index.
 *
 * Returns false if insertion failed, either because
 * there is no space or if the specified index already
 * holds a symbol.
 *
 * If the existing pointer is not-NULL, existing it is
 * set to point to the existing entry.
 *
 * Note that this takes a symbol reference on successful insertion.
 */
static inline
bool symbol_table_insert(struct symbol_table *symtab, uint64_t idx,
                         struct symbol *sym, struct symbol **existing)
{
    if (!table_insert(&symtab->tbl, idx, (void*) symbol_get(sym), (void**) existing)) {
        symbol_put(sym);
        return false;
    }
    symtab->nsymbols++;
    symtab->capacity = symtab->tbl.capacity;
    return true;
}


/*
 * Remove the symbol at the specified index.
 * Note that this releases the symbol reference/
 */
static inline
void symbol_table_remove(struct symbol_table *symtab, uint64_t idx)
{
    struct symbol *sym = table_remove(&symtab->tbl, idx);

    if (sym != NULL) {
        symbol_put(sym);
        symtab->nsymbols--;
    }
}


/*
 * Clear the symbol table and release all sections.
 */
static inline
void symbol_table_clear(struct symbol_table *symtab)
{
    uint64_t idx;

    for (idx = 0; symtab->nsymbols > 0 && idx < symtab->tbl.capacity; ++idx) {
        symbol_table_remove(symtab, idx);
    }

    table_clear(&symtab->tbl);
    symtab->capacity = 0;
}


#ifdef __cplusplus
}
#endif
#endif
