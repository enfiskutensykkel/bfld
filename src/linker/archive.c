#include "archive.h"
#include "logging.h"
#include "objfile.h"
#include "mfile.h"
#include <utils/rbtree.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


static void archive_remove_symbol(struct archive_symbol *symbol)
{
    struct archive *ar = symbol->archive;
    rb_remove(&ar->symbols, &symbol->map_entry);
    free(symbol->name);
    free(symbol);
}


static void archive_remove_member(struct archive_member *member)
{
    struct archive *ar = member->archive;

    rb_remove(&ar->members, &member->map_entry);
    if (member->objfile != NULL) {
        objfile_put(member->objfile);
    }

    if (member->name != NULL) {
        free(member->name);
    }
    free(member);
}


struct archive_member * archive_get_member(const struct archive *ar, size_t offset)
{
    const struct rb_node *node = ar->members.root;

    while (node != NULL) {
        struct archive_member *m = rb_entry(node, struct archive_member, map_entry);

        if (offset < m->offset) {
            node = node->left;
        } else if (offset > m->offset) {
            node = node->right;
        } else {
            return m;
        }
    }

    return NULL;
}


struct archive_member * archive_add_member(struct archive *ar, 
                                           const char *name,
                                           size_t offset,
                                           size_t size)
{
    if (offset + size > ar->file_size) {
        log_error("Invalid offset and size");
        return NULL;
    }

    struct rb_node **pos = &(ar->members.root), *parent = NULL;

    while (*pos != NULL) {
        struct archive_member *this = rb_entry(*pos, struct archive_member, map_entry);
        parent = *pos;

        if (offset < this->offset) {
            pos = &((*pos)->left);
        } else if (offset > this->offset) {
            pos = &((*pos)->right);
        } else {
            log_error("Duplicate archive member at offset %lu", offset);
            return NULL;
        }
    }

    struct archive_member *member = malloc(sizeof(struct archive_member));
    if (member == NULL) {
        return NULL;
    }

    if (name != NULL) {
        member->name = strdup(name);
    }

    member->archive = ar;
    member->offset = offset;
    member->size = size;
    member->content = ar->file_data + offset;
    member->objfile = NULL;

    rb_insert_node(&member->map_entry, parent, pos);
    rb_insert_fixup(&ar->members, &member->map_entry);
    return member;
}


bool archive_add_symbol(struct archive *ar, const char *symbol, size_t offset)
{
    struct rb_node **pos = &(ar->symbols.root), *parent = NULL;

    struct archive_member *member = archive_get_member(ar, offset);
    if (member == NULL) {
        log_fatal("Symbol '%s' in index refers to non-existing archive member %lu",
                symbol, offset);
        return false;
    }

    while (*pos != NULL) {
        struct archive_symbol *this = rb_entry(&pos, struct archive_symbol, map_entry);
        int result = strcmp(symbol, this->name);

        parent = *pos;

        if (result < 0) {
            pos = &((*pos)->left);
        } else {
            // We don't care about duplicates, it's only an index anyway
            pos = &((*pos)->right);
        }
    }

    struct archive_symbol *sym = malloc(sizeof(struct archive_symbol));
    if (sym == NULL) {
        return false;
    }

    sym->name = strdup(symbol);
    if (sym->name == NULL) {
        free(sym);
        return false;
    }

    sym->archive = ar;
    sym->member = member;
    rb_insert_node(&sym->map_entry, parent, pos);
    rb_insert_fixup(&ar->symbols, &sym->map_entry);
    return true;
}


void archive_put(struct archive *ar)
{
    assert(ar != NULL);
    assert(ar->refcnt > 0);

    if (--(ar->refcnt) == 0) {
        
        while (ar->symbols.root != NULL) {
            struct rb_node *node = ar->symbols.root;
            struct archive_symbol *sym = rb_entry(node, struct archive_symbol, map_entry);
            archive_remove_symbol(sym);
        }

        while (ar->members.root != NULL) {
            struct rb_node *node = ar->members.root;
            struct archive_member *member = rb_entry(node, struct archive_member, map_entry);
            archive_remove_member(member);
        }

        mfile_put(ar->file);
        free(ar->name);
        free(ar);
    }
}


struct archive * archive_get(struct archive *ar)
{
    assert(ar != NULL);
    assert(ar->refcnt > 0);
    ++(ar->refcnt);
    return ar;
}


struct archive_member * archive_find_symbol(const struct archive *ar, const char *symbol)
{
    const struct rb_node *node = ar->symbols.root;

    log_ctx_new(ar->name);

    while (node != NULL) {
        struct archive_symbol *this = rb_entry(node, struct archive_symbol, map_entry);
        int result = strcmp(symbol, this->name);

        if (result < 0) {
            node = node->left;
        } else if (result > 0) {
            node = node->right;
        } else {
            log_trace("Found symbol '%s' in member %s", this->name, this->member->name);
            log_ctx_pop();
            return this->member;
        }
    }

    log_ctx_pop();
    return NULL;
}


struct archive * archive_alloc(struct mfile *file,
                               const char *name,
                               const uint8_t *file_data,
                               size_t file_size)
{
    if (file_data == NULL) {
        file_data = (const uint8_t*) file->data;
        file_size = file->size;
    }

    if (file_data < ((const uint8_t*) file->data) || 
            (file_data + file_size) > (((const uint8_t*) file->data) + file->size)) {
        log_fatal("File data content is outside valid range");
        return NULL;
    }

    struct archive *ar = malloc(sizeof(struct archive));
    if (ar == NULL) {
        return NULL;
    }

    ar->name = strdup(name);
    if (ar->name == NULL) {
        free(ar);
        return NULL;
    }

    ar->file = mfile_get(file);
    ar->refcnt = 1;
    ar->file_data = file_data;
    ar->file_size = file_size;
    rb_tree_init(&ar->symbols);
    rb_tree_init(&ar->members);
    return ar;
}
