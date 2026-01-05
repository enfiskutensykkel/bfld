#include "globals.h"
#include "symbol.h"
#include "logging.h"
#include "utils/rbtree.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>


struct globals * globals_alloc(const char *name)
{
    struct globals *globals = malloc(sizeof(struct globals));
    if (globals == NULL) {
        return NULL;
    }

    globals->name = strdup(name);
    if (globals->name == NULL) {
        free(globals);
        return NULL;
    }

    globals->refcnt = 1;
    globals->nsymbols = 0;
    rb_tree_init(&globals->map);
    return globals;
}


struct globals * globals_get(struct globals *globals)
{
    assert(globals != NULL);
    assert(globals->refcnt > 0);
    ++(globals->refcnt);
    return globals;
}


void globals_put(struct globals *globals)
{
    assert(globals != NULL);
    assert(globals->refcnt > 0);

    if (--(globals->refcnt) == 0) {
        while (globals->map.root != NULL) {
            struct rb_node *node = globals->map.root;
            struct globals_entry *entry = rb_entry(node, struct globals_entry, map_entry);
            
            rb_remove(&globals->map, &entry->map_entry);
            symbol_put(entry->symbol);
            free(entry);
        }

        free(globals->name);
        free(globals);
    }
}


int globals_insert_symbol(struct globals *globals, struct symbol *symbol,
                         struct symbol **existing)
{
    struct rb_node **pos = &(globals->map.root), *parent = NULL;

    while (*pos != NULL) {
        struct globals_entry *this = rb_entry(*pos, struct globals_entry, map_entry);
        parent = *pos;

        int result = strcmp(symbol->name, this->symbol->name);
        if (result < 0) {
            pos = &((*pos)->left);
        } else if (result > 0) {
            pos = &((*pos)->right);
        } else {

            if (existing != NULL) {
                *existing = this->symbol;
            }

            return EEXIST;
        }
    }

    struct globals_entry *entry = malloc(sizeof(struct globals_entry));
    if (entry == NULL) {
        return ENOMEM;
    }

    entry->globals = globals;
    entry->symbol = symbol_get(symbol);
    rb_insert_node(&entry->map_entry, parent, pos);
    rb_insert_fixup(&globals->map, &entry->map_entry);

    ++(globals->nsymbols);

    return 0;
}


struct symbol * globals_find_symbol(const struct globals *globals, const char *name)
{
    struct rb_node *node = globals->map.root;

    while (node != NULL) {
        struct globals_entry *entry = rb_entry(node, struct globals_entry, map_entry);

        int result = strcmp(name, entry->symbol->name);
        if (result < 0) {
            node = node->left;
        } else if (result > 0) {
            node = node->right;
        } else {
            return entry->symbol;
        }
    }

    return NULL;
}
