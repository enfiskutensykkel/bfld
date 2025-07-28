#include "archive.h"
#include "archive_loader.h"
#include "logging.h"
#include "objfile.h"
#include "objfile_loader.h"
#include "mfile.h"
#include <utils/list.h>
#include <utils/rbtree.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "pluginregistry.h"


/*
 * List of archive file loaders.
 */
static struct list_head archive_loaders = LIST_HEAD_INIT(archive_loaders);


__attribute__((destructor(65535)))
static void unregister_loaders(void)
{
    plugin_clear_registry(&archive_loaders);
}


int archive_loader_register(const struct archive_loader *loader)
{
    return plugin_register(&archive_loaders, loader->name, loader);
}


const struct archive_loader * archive_loader_find(const char *name)
{
    struct plugin_registry_entry *entry;
    entry = plugin_find_entry(&archive_loaders, name);

    if (entry == NULL) {
        return NULL;
    }

    return (struct archive_loader*) entry->plugin;
}


const struct archive_loader * archive_loader_probe(const uint8_t *data, size_t size)
{
    list_for_each_entry(loader_entry, &archive_loaders, struct plugin_registry_entry, list_node) {
        const struct archive_loader *loader = loader_entry->plugin;
        if (loader->probe(data, size)) {
            return loader;
        }
    }

    return NULL;
}


static void archive_member_remove(struct archive *ar, struct archive_member *member)
{
    rb_remove(&ar->members, &member->tree_node);

    if (member->objfile != NULL) {
        objfile_put(member->objfile);
    }

    if (member->name != NULL) {
        free(member->name);
    }

    free(member);
}


static int archive_member_create(struct archive *ar, uint64_t member_id, const char *name,
                                 size_t offset, size_t size)
{
    struct rb_node **pos = &(ar->members.root), *parent = NULL;

    while (*pos != NULL) {
        struct archive_member *this = rb_entry(*pos, struct archive_member, tree_node);
        parent = *pos;

        if (member_id < this->member_id) {
            pos = &((*pos)->left);
        } else if (member_id > this->member_id) {
            pos = &((*pos)->right);
        } else {
            log_error("Duplicate archive member index %lu", member_id);
            return EINVAL;
        }
    }

    struct archive_member *member = malloc(sizeof(struct archive_member));
    if (member == NULL) {
        return ENOMEM;
    }

    if (name != NULL) {
        member->name = strdup(name);
        if (member->name == NULL) {
            free(member);
            return ENOMEM;
        }
    }

    member->member_id = member_id;
    member->size = size;
    member->offset = offset;
    member->objfile = NULL;

    rb_insert_node(&member->tree_node, parent, pos);
    rb_insert_fixup(&ar->members, &member->tree_node);
    return 0;
}


static void archive_symbol_remove(struct archive *ar, struct archive_symbol *symbol)
{
    rb_remove(&ar->symbols, &symbol->tree_node);
    free(symbol->name);
    free(symbol);
}


struct archive_symbol * archive_lookup_symbol(const struct archive *ar, const char *name)
{
    const struct rb_node *node = ar->symbols.root;

    while (node != NULL) {
        struct archive_symbol *this = rb_entry(node, struct archive_symbol, tree_node);
        int result = strcmp(name, this->name);

        if (result < 0) {
            node = node->left;
        } else if (result > 0) {
            node = node->right;
        } else {
            return this;
        }
    }

    return NULL;
}


struct archive_member * archive_lookup_member(const struct archive *ar, uint64_t member_id)
{
    const struct rb_node *node = ar->members.root;

    while (node != NULL) {
        struct archive_member *member = rb_entry(node, struct archive_member, tree_node);

        if (member_id < member->member_id) {
            node = node->left;
        } else if (member_id > member->member_id) {
            node = node->right;
        } else {
            return member;
        }
    }

    return NULL;
}


static int archive_symbol_create(struct archive *ar, const char *name, uint64_t member_id)
{
    struct rb_node **pos = &(ar->symbols.root), *parent = NULL;

    struct archive_member *member = archive_lookup_member(ar, member_id);
    if (member == NULL) {
        log_error("Indexed symbol '%s' refers to non-existing archive member %lu", name, member_id);
        return EINVAL;
    }

    while (*pos != NULL) {
        struct archive_symbol *this = rb_entry(*pos, struct archive_symbol, tree_node);
        int result = strcmp(name, this->name);
        parent = *pos;

        if (result < 0) {
            pos = &((*pos)->left);
        } else {
            // we don't care about duplicates, it's only an index anyway
            pos = &((*pos)->right);
        }
    }

    struct archive_symbol *sym = malloc(sizeof(struct archive_symbol));
    if (sym == NULL) {
        return ENOMEM;
    }

    sym->name = strdup(name);
    if (sym->name == NULL) {
        free(sym);
        return ENOMEM;
    }
    log_debug("create %s %p", sym->name, sym);

    sym->member_id = member_id;
    sym->member = member;
    
    rb_insert_node(&sym->tree_node, parent, pos);
    rb_insert_fixup(&ar->symbols, &sym->tree_node);

    return 0;
}


void archive_put(struct archive *ar)
{
    if (--(ar->refcnt) == 0) {
        struct rb_node *node, *next;

        for (node = rb_first(&ar->symbols), next = rb_next(node); 
                node != NULL; 
                node = next, next = next != NULL ? rb_next(next) : NULL) {
            struct archive_symbol *sym = rb_entry(node, struct archive_symbol, tree_node);
            log_debug("delete %s", sym->name);
            archive_symbol_remove(ar, sym);
        }

        for (node = rb_first(&ar->members), next = rb_next(node); 
                node != NULL; 
                node = next, next = next != NULL ? rb_next(next) : NULL) {
            struct archive_member *member = rb_entry(node, struct archive_member, tree_node);
            archive_member_remove(ar, member);
        }

        if (ar->loader != NULL && ar->loader->release != NULL) {
            ar->loader->release(ar->loader_data);
        }
        mfile_put(ar->file);
        free(ar->name);
        free(ar);
    }
}


void archive_get(struct archive *ar)
{
    ++(ar->refcnt);
}


int archive_init(struct archive **ar, mfile *file, const char *name)
{
    *ar = NULL;

    struct archive *a = malloc(sizeof(struct archive));
    if (a == NULL) {
        return ENOMEM;
    }

    a->name = strdup(name);
    if (a->name == NULL) {
        free(a);
        return ENOMEM;
    }

    mfile_get(file);
    a->file = file;
    a->refcnt = 1;
    a->loader_data = NULL;
    a->loader = NULL;

    rb_tree_init(&a->symbols);
    rb_tree_init(&a->members);

    *ar = a;
    return 0;
}


static bool _archive_member_create(void *ctx, uint64_t member_id, const char *name,
                                   size_t offset, size_t size)
{
    int status = archive_member_create(ctx, member_id, name, offset, size);
    return status == 0;
}


static bool _archive_symbol_create(void *ctx, const char *name, uint64_t member_id)
{
    int status = archive_symbol_create(ctx, name, member_id);
    return status == 0;
}


struct archive * archive_load(mfile *file, const struct archive_loader *loader)
{
    if (file == NULL) {
        return NULL;
    }

    if (loader == NULL) {
        loader = archive_loader_probe(file->data, file->size);
    }

    if (loader == NULL) {
        return NULL;
    }

    log_ctx_push(LOG_CTX_FILE(loader->name, file->name));

    struct archive *ar = NULL;
    int status = archive_init(&ar, file, file->name);
    if (status != 0) {
        log_ctx_pop();
        return NULL;
    }

    status = loader->parse_file(&ar->loader_data, file->data, file->size);
    if (status != 0) {
        log_error("Invalid file format or corrupt file");
        archive_put(ar);
        log_ctx_pop();
        return NULL;
    }

    ar->loader = loader;

    status = ar->loader->parse_members(ar->loader_data, _archive_member_create, ar); 
    if (status != 0) {
        log_error("Could not parse archive members");
        archive_put(ar);
        log_ctx_pop();
        return NULL;
    }

    status = ar->loader->parse_symbol_index(ar->loader_data, _archive_symbol_create, ar);
    if (status != 0) {
        log_error("Could not parse archive symbol index");
        archive_put(ar);
        log_ctx_pop();
        return NULL;
    }

    log_ctx_pop();
    return ar;
}
