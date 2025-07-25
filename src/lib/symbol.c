#include "symbol.h"
#include "section.h"
#include <utils/rbtree.h>
#include <stdlib.h>
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


int symbol_create(struct symbol **sym, struct rb_tree *symtab, const char *name)
{
    *sym = NULL;

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
            return EEXIST;
        }
    }

    struct symbol *s = malloc(sizeof(struct symbol));
    if (s == NULL) {
        return ENOMEM;
    }

    s->name = strdup(name);
    if (s->name == NULL) {
        free(s);
        return ENOMEM;
    }

    s->section = NULL;
    s->offset = 0;

    rb_insert_node(&s->tree_node, parent, pos);
    rb_insert_fixup(symtab, &s->tree_node);
    *sym = s;
    return 0;
}


void symbol_remove(struct rb_tree *symtab, struct symbol **sym)
{
    if (*sym != NULL) {
        rb_remove(symtab, &(*sym)->tree_node);

        if ((*sym)->section != NULL) {
            // section_put((*sym)->section);
            (*sym)->section = NULL;
        }

        free((*sym)->name);
        free(*sym);
        *sym = NULL;
    }
}
