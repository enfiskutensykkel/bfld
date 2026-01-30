#include "table.h"
#include "align.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


bool table_reserve(struct table *tbl, uint64_t capacity)
{
    if (capacity <= tbl->capacity) {
        return true;
    }

    // Make sure capacity is aligned to a power of two
    capacity = align_roundup(capacity);
    if (capacity < 8) {
        capacity = 8;
    }

    // Naive check for overflow
    if (capacity * sizeof(void*) < tbl->capacity * sizeof(void*)) {
        return false;
    }

    void **table = realloc(tbl->table, sizeof(void*) * capacity);
    if (table == NULL) {
        return false;
    }

    memset(&table[tbl->capacity], 0, (capacity - tbl->capacity) * sizeof(void*));
    tbl->table = table;
    tbl->capacity = capacity;
    return true;
}


void table_clear(struct table *tbl)
{
    if (tbl->table != NULL) {
        free(tbl->table);
    }
    table_init(tbl);
}
