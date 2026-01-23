#ifndef BFLD_UTILS_TABLE_H
#define BFLD_UTILS_TABLE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


struct table
{
    void **table;
    uint64_t capacity;
};


#define TABLE_INIT (struct table) {NULL, 0}


bool table_reserve(struct table *t, uint64_t capacity);


static inline
void table_init(struct table *t)
{
    t->table = NULL;
    t->capacity = 0;
}


static inline
void * table_at(const struct table *t, uint64_t index)
{
    void *entry = NULL;

    if (index < t->capacity) {
        entry = t->table[index];
    }

    return entry;
}


static inline
bool table_insert(struct table *t, uint64_t index, void *entry, void **existing)
{
    void **e = NULL;

    if (index >= t->capacity) {
        if (!table_reserve(t, index + 1)) {
            return false;
        }
    }

    e = (&t->table[index]);
    if (*e != NULL) {
        if (existing != NULL) {
            *existing = *e;
        }
        return false;
    }

    *e = entry;
    return true;
}


static inline
void * table_remove(struct table *t, uint64_t index)
{
    void *entry = NULL;

    if (index >= t->capacity) {
        return NULL;
    }

    entry = t->table[index];
    if (entry == NULL) {
        return NULL;
    }

    t->table[index] = NULL;
    return entry;
}


void table_clear(struct table *t);


#define TABLE_DECLARE(type, name) \
    struct name \
    { \
        type **table; \
        uint64_t capacity; \
    }; \
    static inline \
    bool name##_reserve(struct name *tbl, uint64_t capacity) \
    { \
        return table_reserve((struct table*) tbl, capacity); \
    } \
    static inline \
    type * name##_at(const struct name *t, uint64_t index) \
    { \
        return (type*) table_at((const struct table*) t, index); \
    } \
    static inline \
    bool name##_insert(struct name *t, uint64_t index, type *entry, type **existing) \
    { \
        return table_insert((struct table*) t, index, (void*) entry, (void**) existing); \
    } \
    static inline \
    type * name##_remove(struct name *t, uint64_t index) \
    { \
        return (type*) table_remove((struct table*) t, index); \
    } \
    static inline \
    void name##_clear(struct name *t) \
    { \
        table_clear((struct table*) t); \
    }
    

#ifdef __cplusplus
}
#endif
#endif
