#include "symtypes.h"
#include "symtab.h"
#include "section.h"
#include "objfile.h"
#include "utils/list.h"
#include "utils/rbtree.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "logging.h"


int symtab_init(struct symtab **symtab, const char *name)
{
    *symtab = NULL;

    struct symtab *st = malloc(sizeof(struct symtab));
    if (st == NULL) {
        return ENOMEM;
    }

    if (st != NULL) {
        st->name = strdup(name);
        if (st->name == NULL) {
            free(st);
            return ENOMEM;
        }
    }

    st->refcnt = 1;
    rb_tree_init(&st->tree);
    *symtab = st;
    return 0;
}


void symtab_get(struct symtab *symtab)
{
    ++(symtab->refcnt);
}


void symtab_put(struct symtab *symtab)
{
    if (--(symtab->refcnt) == 0) {
        struct rb_node *node = rb_first_postorder(&symtab->tree);

        while (node != NULL) {
            struct rb_node *next = rb_next_postorder(node);
            struct symbol *sym = rb_entry(node, struct symbol, tree_node);
            assert(sym->table == symtab);
            symbol_free(sym);
            node = next;
        }

        if (symtab->name != NULL) {
            free(symtab->name);
        }
        free(symtab);
    }
}


struct symbol * symtab_find_symbol(const struct symtab *symtab, const char *name)
{
    const struct rb_node *node = symtab->tree.root;

    while (node != NULL) {
        const struct symbol *sym = rb_entry(node, struct symbol, tree_node);

        int result = strcmp(name, sym->name);
        if (result < 0) {
            node = node->left;
        } else if (result > 0) {
            node = node->right;
        } else {
            return sym;
        }
    }

    return NULL;
}


int symtab_remove_symbol(struct symtab *symtab, struct symbol *sym)
{
    if (sym->table != symtab) {
        return EINVAL;
    }

    rb_remove(&symtab->tree, &sym->tree_node);
    rb_node_init(&sym->tree_node);
    sym->table = NULL;
    return 0;
}


int symtab_replace_symbol(struct symtab *symtab, struct symbol *victim, 
                          struct symbol *replacement)
{
    if (replacement->table != NULL || rb_node_is_inserted(&replacement->tree_node)) {
        return EINVAL;
    } else if (symtab != victim->table) {
        return EINVAL;
    } else if (strcmp(victim->name, replacement->name) != 0) {
        return EINVAL;
    }

    rb_replace_node(&symtab->tree, &victim->tree_node, &replacement->tree_node);
    replacement->table = symtab;
    rb_node_init(&victim->tree_node);
    return 0;
}


int symtab_insert_symbol(struct symtab *symtab, struct symbol *sym, 
                         struct symbol **existing)
{
    struct rb_node **pos = &(symtab->tree.root), *parent = NULL;

    if (existing != NULL) {
        *existing = NULL;
    }

    if (sym->table != NULL || rb_node_is_inserted(&sym->tree_node)) {
        return EINVAL;
    }
    
    while (*pos != NULL) {
        struct symbol *this = rb_entry(*pos, struct symbol, tree_node);
        int result = strcmp(sym->name, this->name);

        parent = *pos;
        if (result < 0) {
            pos = &((*pos)->left);
        } else if (result > 0) {
            pos = &((*pos)->right);
        } else {
            // A symbol with the same name alread exists
            if (existing != NULL) {
                *existing = this;
            }
            return EEXIST;
        }
    }

    rb_insert_node(&sym->tree_node, parent, pos);
    rb_insert_fixup(&symtab->tree, &sym->tree_node);
    sym->table = symtab;
    return 0;
}


void symbol_free(struct symbol *sym)
{
    if (sym->table != NULL) {
        rb_remove(&(sym->table->tree), &sym->tree_node);
    }

    list_for_each_entry_safe(ref, &sym->refs, struct symref, list_node) {
        list_remove(&ref->list_node);
        objfile_put(ref->referer);
        free(ref);
    }

    if (sym->definer != NULL) {
        objfile_put(sym->definer);
    }

    free(sym->name);
    free(sym);
}


int symbol_alloc(struct symbol **sym, struct objfile *referer,
                 const char *name, bool weak)
{
    *sym = NULL;

    struct symbol *s = malloc(sizeof(struct symbol));
    if (s == NULL) {
        return ENOMEM;
    }

    s->name = strdup(name);
    if (s->name == NULL) {
        free(s);
        return ENOMEM;
    }

    struct symref *ref = malloc(sizeof(struct symref));
    if (ref == NULL) {
        free(s->name);
        free(s);
        return ENOMEM;
    }

    s->table = NULL;
    rb_node_init(&s->tree_node);
    s->weak = weak;
    s->type = SYMBOL_NOTYPE;
    s->addr = 0;
    s->definer = NULL;
    s->offset = 0;
    s->size = 0;
    list_head_init(&s->refs);

    list_insert_tail(&s->refs, &ref->list_node);
    objfile_get(referer);
    ref->referer = referer;
    ref->symbol = s;

    *sym = s;
    return 0;
}


int symbol_resolve(struct symbol *sym, struct objfile *objfile,
                   uint64_t sect_idx, size_t offset, 
                   enum symbol_type type, size_t size)
{
    if (sym->definer != NULL) {
        return EINVAL;
    }

    sym->definer = objfile;
    objfile_get(objfile);

    sym->sect_idx = sect_idx;
    sym->offset = offset;
    sym->type = type;
    sym->size = size;

    return 0;
}


struct symref * symbol_add_reference(struct symbol *sym, struct objfile *file)
{
    struct symref *ref = malloc(sizeof(struct symref));
    if (ref == NULL) {
        return NULL;
    }

    ref->symbol = sym;
    ref->referer = file;
    objfile_get(file);
    list_insert_tail(&sym->refs, &ref->list_node);

    return ref;
}
