#include "secttab.h"
#include "section.h"
#include "logging.h"
#include "utils/rbtree.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>


static int extend_capacity(struct secttab *st)
{
    size_t new_capacity = st->capacity * 2;

    log_ctx_push(LOG_CTX_NAME(st->name));
    log_trace("extending section table capacity");
    log_ctx_pop();

    // Sanity check that we're not overflowing
    if ((new_capacity * sizeof(struct section*)) < (st->capacity * sizeof(struct section*))) {
        return ENOMEM;
    }

    struct section **entries = realloc(st->sections, sizeof(struct section*) * new_capacity);
    if (entries == NULL) {
        return ENOMEM;
    }

    // Zero out the new entries
    st->sections = entries;
    while (st->capacity < new_capacity) {
        st->sections[st->capacity] = NULL;
        ++(st->capacity);
    }

    return 0;
}


struct secttab * secttab_alloc(const char *name)
{
    struct secttab *st = malloc(sizeof(struct secttab));
    if (st == NULL) {
        return NULL;
    }

    st->name = strdup(name);
    if (st->name == NULL) {
        free(st);
        return NULL;
    }

    st->capacity = 256;
    st->sections = calloc(st->capacity, sizeof(struct section*));
    if (st->sections == NULL) {
        free(st->name);
        free(st);
        return NULL;
    }
    st->nsections = 0;

    st->refcnt = 1;
    rb_tree_init(&st->name_map);
    return st;
}


struct secttab * secttab_get(struct secttab *st)
{
    assert(st != NULL);
    assert(st->refcnt > 0);
    ++(st->refcnt);
    return st;
}


void secttab_put(struct secttab *st)
{
    assert(st != NULL);
    assert(st->refcnt > 0);
    if (--(st->refcnt) == 0) {
        while (st->name_map.root != NULL) {
            struct rb_node *node = st->name_map.root;
            struct secttab_entry *entry = rb_entry(node, struct secttab_entry, map_entry);
            rb_remove(&st->name_map, &entry->map_entry);
            free(entry);
        }
        free(st->sections);
        free(st->name);
        free(st);
    }
}


bool secttab_insert_section(struct secttab *st, uint64_t idx, struct section *sect)
{
    struct rb_node **pos = &(st->name_map.root), *parent = NULL;

    log_ctx_push(LOG_CTX_NAME(st->name));

    while (*pos != NULL) {
        struct secttab_entry *this = rb_entry(*pos, struct secttab_entry, map_entry);

        int result = strcmp(sect->name, this->section->name);
        if (result < 0) {
            pos = &((*pos)->left);
        } else if (result > 0) {
            pos = &((*pos)->right);
        } else {
            // We will allow sections with the same name (as long as
            // they have unique indices), but it is not ideal
            log_warning("Section table already contains a section with name '%s'",
                    sect->name);
            pos = &((*pos)->right);
        }
    }

    while (idx > st->capacity) {
        if (extend_capacity(st) != 0) {
            log_error("Section index %llu is too large", idx);
            log_ctx_pop();
            return false;
        }
    }

    if (st->sections[idx] != NULL) {
        log_error("Section table already contains a section with index %llu", idx);
        log_ctx_pop();
        return false;
    }

    struct secttab_entry *entry = malloc(sizeof(struct secttab_entry));
    if (entry == NULL) {
        log_ctx_pop();
        return false;
    }

    entry->secttab = st;
    entry->section_idx = idx;
    entry->section = section_get(sect);
    rb_insert_node(&entry->map_entry, parent, pos);
    rb_insert_fixup(&st->name_map, &entry->map_entry);
    ++(st->nsections);
    st->sections[entry->section_idx] = entry->section;

    log_ctx_pop();
    return true;
}


struct section * secttab_get_section(struct secttab *st, uint64_t idx)
{
    if (idx >= st->capacity) {
        return NULL;
    }

    return st->sections[idx];
}


struct section * secttab_get_section_by_name(struct secttab *st, const char *name)
{
    const struct rb_node *node = st->name_map.root;

    while (node != NULL) {
        struct secttab_entry *this = rb_entry(node, struct secttab_entry, map_entry);
        int result = strcmp(name, this->section->name);

        if (result < 0) {
            node = node->left;
        } else if (result > 0) {
            node = node->right;
        } else {
            return this->section;
        }
    }

    return NULL;
}
