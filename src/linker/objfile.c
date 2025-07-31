#include "arch.h"
#include "logging.h"
#include "secttypes.h"
#include "symtypes.h"
#include "objfile.h"
#include "objfile_loader.h"
#include "mfile.h"
#include "pluginregistry.h"
#include "utils/list.h"
#include "utils/rbtree.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>


/*
 * Helper struct to pass as user-data when invoking 
 * callbacks on the objfile_loader.
 */
struct objfile_callback_data
{
    int status;
    struct objfile *objfile;
    objfile_syminfo_cb emit_symbol_cb;
    objfile_relinfo_cb emit_reloc_cb;
    void *cb_data;
};


/*
 * List of object file loaders.
 */ 
static struct list_head objfile_loaders = LIST_HEAD_INIT(objfile_loaders);


__attribute__((destructor(65535)))
static void unregister_loaders(void)
{
    plugin_clear_registry(&objfile_loaders);
}


int objfile_loader_register(const struct objfile_loader *loader)
{
    if (loader == NULL) {
        return EINVAL;
    }

    if (loader->name == NULL) {
        return EINVAL;
    }

    if (loader->probe == NULL) {
        return EINVAL;
    }

    if (loader->parse_file == NULL || loader->parse_sections == NULL || loader->release == NULL) {
        return EINVAL;
    }

    if (loader->extract_symbols == NULL || loader->extract_relocations == NULL) {
        return EINVAL;
    }

    return plugin_register(&objfile_loaders, loader->name, loader);
}


const struct objfile_loader * objfile_loader_find(const char *name)
{
    struct plugin_registry_entry *entry;
    entry = plugin_find_entry(&objfile_loaders, name);

    if (entry == NULL) {
        return NULL;
    }

    return (struct objfile_loader*) entry->plugin;
}


const struct objfile_loader * objfile_loader_probe(const uint8_t *data, size_t size)
{
    enum arch_type arch = ARCH_UNKNOWN;

    list_for_each_entry(loader_entry, &objfile_loaders, struct plugin_registry_entry, list_node) {
        const struct objfile_loader *loader = loader_entry->plugin;
        if (loader->probe(data, size, &arch)) {
            return loader;
        }
    }

    return NULL;
}


static struct section * objfile_find_section(const struct objfile *objfile, uint64_t key)
{
    if (key == 0) {
        return NULL;
    }

    struct rb_node *node = objfile->sections.root;

    while (node != NULL) {
        struct section *sect = rb_entry(node, struct section, tree_node);

        if (key < sect->key) {
            node = node->left;
        } else if (key > sect->key) {
            node = node->right;
        } else {
            return sect;
        }
    }

    return NULL;
}


static struct section * objfile_create_section(struct objfile *objfile, 
                                               uint64_t key,
                                               const char *name)
{
    if (key == 0) {
        log_error("Section identifier can not be 0");
        return NULL;
    }

    struct rb_node **pos = &(objfile->sections.root), *parent = NULL;

    while (*pos != NULL) {
        struct section *this = rb_entry(*pos, struct section, tree_node);
        parent = *pos;
        if (key < this->key) {
            pos = &((*pos)->left);
        } else if (key > this->key) {
            pos = &((*pos)->right);
        } else {
            // Section with the same key already exist
            return NULL;
        }
    }

    struct section *sect = malloc(sizeof(struct section));
    if (sect == NULL) {
        return NULL;
    }

    sect->key = key;
    sect->objfile = objfile;
    if (name != NULL) {
        sect->name = strdup(name);
    }
    sect->type = SECTION_ZERO;
    sect->content = NULL;
    sect->size = 0;
    sect->align = 0;

    rb_insert_node(&sect->tree_node, parent, pos);
    rb_insert_fixup(&objfile->sections, &sect->tree_node);
    ++(objfile->num_sections);
    return sect;
}


static void objfile_remove_section(struct objfile *objfile, struct section *sect)
{
    assert(objfile != NULL && sect->objfile == objfile);

    rb_remove(&objfile->sections, &sect->tree_node);

    if (sect->name != NULL) {
        free(sect->name);
    }

    free(sect);
    --(objfile->num_sections);
}


void objfile_put(struct objfile *objfile)
{
    if (--(objfile->refcnt) == 0) {
        while (objfile->sections.root != NULL) {
            struct section *sect = rb_entry(objfile->sections.root, struct section, tree_node);
            objfile_remove_section(objfile, sect);
        }

        if (objfile->loader != NULL && objfile->loader->release != NULL) {
            objfile->loader->release(objfile->loader_data);
        }

        if (objfile->file != NULL) {
            mfile_put(objfile->file);
        }

        free(objfile->name);
        free(objfile);
    }
}


void objfile_get(struct objfile *objfile)
{
    ++(objfile->refcnt);
}


bool _add_section(void *ctx, const struct objfile_section *sect)
{
    struct section *s = objfile_create_section(ctx, sect->section, sect->name);
    if (s == NULL) {
        return false;
    } 

    s->type = sect->type;
    s->size = sect->size;
    s->align = sect->align;
    s->offset = sect->offset;
    s->content = sect->size > 0 ? sect->content : NULL;
    return true;
}


int objfile_init(struct objfile **objfile, const struct objfile_loader *loader,
                 const char *name, const uint8_t *data, size_t size)
{
    struct objfile *obj = malloc(sizeof(struct objfile));
    if (obj == NULL) {
        return ENOMEM;
    }

    obj->name = strdup(name);
    if (obj->name == NULL) {
        free(obj);
        return ENOMEM;
    }

    if (!loader->probe(data, size, &obj->arch)) {
        free(obj->name);
        free(obj);
        return EINVAL;
    }

    obj->file = NULL;
    obj->refcnt = 1;

    obj->loader = NULL;
    obj->loader_data = NULL;
    obj->num_sections = 0;
    rb_tree_init(&obj->sections);

    struct section *common = objfile_create_section(obj, 0xdeadbeef, ".common");
    if (common == NULL) {
        objfile_put(obj);
        return ENOMEM;
    }

    log_ctx_push(LOG_CTX_FILE(loader->name, name));

    log_trace("Parsing object file");

    // Do the initial parsing of the file
    int status = loader->parse_file(&obj->loader_data, data, size, 
                                    &obj->handler);
    if (status != 0) {
        // Loader wasn't happy with the file
        log_error("Invalid file format or corrupt file");
        objfile_put(obj);
        log_ctx_pop();
        return EINVAL;
    }

    obj->loader = loader;

    // Read section metadata
    status = loader->parse_sections(obj->loader_data, _add_section, obj);
    if (status != 0) {
        log_error("Unable to parse sections");
        objfile_put(obj);
        log_ctx_pop();
        return EINVAL;
    }

    log_ctx_pop();
    *objfile = obj;
    return 0;
}


struct objfile * objfile_load(mfile *file, const struct objfile_loader *loader)
{
    if (file == NULL) {
        return NULL;
    }

    if (loader == NULL) {
        loader = objfile_loader_probe(file->data, file->size);
    }

    if (loader == NULL) {
        // We could not find any suitable loaders
        return NULL;
    }

    struct objfile *objfile = NULL;
    int status = objfile_init(&objfile, loader, 
                              file->name, file->data, file->size);
    if (status != 0) {
        return NULL;
    }

    mfile_get(file);
    objfile->file = file;
    return objfile;
}


static bool _emit_symbol(void *cb_data, const struct objfile_symbol *sym)
{
    struct objfile_callback_data *cb = cb_data;
    struct objfile *objfile = cb->objfile;

    struct section *sect = NULL;
    if (sym->common) {
        sect = objfile_find_section(objfile, 0xdeadbeef);
    } else {
        sect = objfile_find_section(objfile, sym->section);
    }

    struct syminfo info = {
        .name = sym->name,
        .is_reference = sect == NULL && sym->relative,
        .global = sym->binding != SYMBOL_LOCAL,
        .weak = sym->binding == SYMBOL_WEAK,
        .type = sym->type,
        .relative = sym->relative,
        .section = sym->relative ? sect : NULL,
        .offset = sym->offset,
    };

    bool _continue = cb->emit_symbol_cb(cb->cb_data, objfile, &info);
    if (!_continue) {
        cb->status = ECANCELED;
    }

    return _continue;
}


static bool _emit_reloc(void *cb_data, const struct objfile_relocation *rel)
{
    struct objfile_callback_data *cb = cb_data;
    struct objfile *objfile = cb->objfile;

    struct section *sect = objfile_find_section(objfile, rel->section);
    if (sect == NULL) {
        log_error("Invalid section in relocation");
        cb->status = EBADF;
        return false;
    }

    struct relinfo info = {
        .section = sect,
        .offset = rel->offset,
        .symbol_name = NULL,
        .section_ref = NULL,
        .type = rel->type,
        .addend = rel->addend
    };

    if (rel->sectionref != 0) {
        struct section *ref = objfile_find_section(objfile, rel->sectionref);
        if (ref == NULL) {
            log_error("Invalid section reference in relocation");
            cb->status = EBADF;
            return false;
        }

        info.section_ref = ref;
    } else if (rel->commonref) {
        struct section *ref = objfile_find_section(objfile, 0xdeadbeef);
        info.section_ref = ref;
    } else {
        info.symbol_name = rel->symbol;
    }

    bool _continue = cb->emit_reloc_cb(cb->cb_data, objfile, &info);
    if (!_continue) {
        cb->status = ECANCELED;
    }

    return true;
}


int objfile_extract_symbols(struct objfile *objfile, objfile_syminfo_cb callback, 
                            void *callback_data)
{
    assert(objfile->loader != NULL && objfile->loader->extract_symbols != NULL);

    log_ctx_push(LOG_CTX_FILE(objfile->loader->name, objfile->name));

    struct objfile_callback_data cbdata = {
        .status = 0,
        .objfile = objfile,
        .emit_symbol_cb = callback,
        .emit_reloc_cb = NULL,
        .cb_data = callback_data
    };

    int status = objfile->loader->extract_symbols(objfile->loader_data, 
                                                  _emit_symbol,
                                                  &cbdata);
    if (cbdata.status != 0) {
        log_ctx_pop();
        return cbdata.status;
    }

    if (status != 0) {
        log_error("Extracting symbols failed");
        log_ctx_pop();
        return EBADF;
    }

    log_ctx_pop();
    return 0;
}


int objfile_extract_relocations(struct objfile *objfile, objfile_relinfo_cb callback,
                                void *callback_data)
{
    assert(objfile->loader != NULL && objfile->loader->extract_relocations != NULL);

    log_ctx_push(LOG_CTX_FILE(objfile->loader->name, objfile->name));

    struct objfile_callback_data cbdata = {
        .status = 0,
        .objfile = objfile,
        .emit_symbol_cb = NULL,
        .emit_reloc_cb = callback,
        .cb_data = callback_data
    };

    int status = objfile->loader->extract_relocations(objfile->loader_data,
                                                      _emit_reloc,
                                                      &cbdata);
    if (cbdata.status != 0) {
        log_ctx_pop();
        return cbdata.status;
    }

    if (status != 0) {
        log_error("Extracting relocations failed");
        log_ctx_pop();
        return EBADF;
    }

    log_ctx_pop();
    return 0;
}
