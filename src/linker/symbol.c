#include "symbol.h"
#include "section.h"
#include "objfile.h"
#include "utils/rbtree.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>


struct symbol * symbol_find(const struct rb_tree *symtab, const char *name)
{
    const struct rb_node *node = symtab->root;

    while (node != NULL) {
        struct symbol *sym = rb_entry(node, struct symbol, tree_node);

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


static void free_symbol(struct symbol *sym)
{
    objfile_put(sym->source);
    if (sym->definition) {
        objfile_put(sym->definition);
    }
    free((void*) sym->name);
    free(sym);
}


int symbol_create(struct symbol **sym, 
                  struct objfile *source,
                  const char *name,
                  enum symbol_binding binding, 
                  enum symbol_type type,
                  struct rb_tree *symtab)
{
    *sym = NULL;

    struct symbol *s = NULL;
    struct rb_node **pos = &(symtab->root), *parent = NULL;
    
    while (*pos != NULL) {
        struct symbol *this = rb_entry(*pos, struct symbol, tree_node);
        int result = strcmp(name, this->name);

        parent = *pos;
        if (result < 0) {
            pos = &((*pos)->left);
        } else if (result > 0) {
            pos = &((*pos)->right);
        } else {
            // A symbol with the specified name already exists
            *sym = this;
            break;
        }
    }

    if ((*sym) != NULL && (*sym)->binding != SYMBOL_WEAK) {
        // The existing symbol is strong
        return EEXIST;
    }

    s = malloc(sizeof(struct symbol));
    if (s == NULL) {
        return ENOMEM;
    }

    s->name = strdup(name);
    if (s->name == NULL) {
        free(s);
        return ENOMEM;
    }

    s->binding = binding;
    s->type = type;
    s->addr = 0;
    objfile_get(source);
    s->source = source;
    s->defined = false;
    s->sect_idx = 0;
    s->offset = 0;
    s->size = 0;
    s->definition = NULL;

    if ((*sym) == NULL) {
        // New symbol, insert it at the given position
        rb_insert_node(&s->tree_node, parent, pos);
        rb_insert_fixup(symtab, &s->tree_node);
    } else {
        // We're replacing a weak symbol, just replace the tree node
        rb_replace_node(symtab, &(*sym)->tree_node, &s->tree_node);
        free_symbol((*sym));
    }
    *sym = s;
    return 0;
}


void symbol_remove(struct rb_tree *symtab, struct symbol **sym)
{
    if (*sym != NULL) {
        rb_remove(symtab, &(*sym)->tree_node);
        free_symbol(*sym);
        *sym = NULL;
    }
}


int symbol_resolve_definition(struct symbol *sym, struct objfile *file,
                              uint64_t sect_idx, size_t offset, size_t size)
{
    if (sym->definition != NULL) {
        return EEXIST;
    }

    sym->defined = true;
    objfile_get(file);
    sym->definition = file;
    sym->sect_idx = sect_idx;
    sym->offset = offset;
    sym->size = size;

    return 0;
}
