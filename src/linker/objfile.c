#include "logging.h"
#include "secttypes.h"
#include "symtypes.h"
#include "objfile.h"
#include "objfile_loader.h"
#include "mfile.h"
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
    bool (*cb)(void*, struct objfile*, const struct symbol_info*);
    void *cb_data;
};


struct loader_entry
{
    struct list_head node;
    const struct objfile_loader *loader;
};


static struct list_head loaders = LIST_HEAD_INIT(loaders);


__attribute__((destructor(65535)))
static void remove_loaders(void)
{
    list_for_each_entry_safe(entry, &loaders, struct loader_entry, node) {
        list_remove(&entry->node);
        free(entry);
    }
}


int objfile_loader_register(const struct objfile_loader *loader)
{
    if (loader == NULL || loader->name == NULL) {
        return EINVAL;
    }

    if (loader->probe == NULL || loader->scan_file == NULL) {
        return EINVAL;
    }

    if (loader->extract_sections == NULL 
            || loader->extract_symbols == NULL 
            || loader->extract_relocations == NULL) {
        return EINVAL;
    }

    struct loader_entry *entry = malloc(sizeof(struct loader_entry));
    if (entry == NULL) {
        return ENOMEM;
    }

    entry->loader = loader;
    list_insert_tail(&loaders, &entry->node);
    return 0;
}


const struct objfile_loader * objfile_loader_probe(const uint8_t *data, size_t size)
{
    list_for_each_entry(loader_entry, &loaders, struct loader_entry, node) {
        const struct objfile_loader *loader = loader_entry->loader;

        if (loader->probe(data, size)) {
            return loader;
        }
    }

    return NULL;
}


static struct section * objfile_find_section(const struct objfile *objfile,
                                             unsigned key)
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
                                               unsigned key,
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
    sect->name = name;
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


static bool _add_section(void *ctx, const struct objfile_section *sect)
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


//static bool _add_relocation(void *ctx, const struct objfile_relocation *reloc)
//{
//    struct section *s = objfile_find_section(ctx, reloc->section);
//
//    if (s == NULL) {
//        log_error("Invalid section in relocation");
//        return false;
//    }
//
//    struct relocation *rel = objfile_create_relocation(ctx, s);
//    if (rel == NULL) {
//        return false;
//    }
//
//    if (reloc->commonref) {
//        rel->target_section = objfile_find_section(ctx, 0xdeadbeef);
//    } else if (reloc->sectionref) {
//        rel->target_section = objfile_find_section(ctx, reloc->sectionref);
//        if (rel->target_section == NULL) {
//            log_error("Invalid section reference in relocation");
//            free(rel);
//            return false;
//        }
//    } else {
//        if (reloc->symbol == NULL) {
//            free(rel);
//            log_error("Expected symbol name for relocation");
//            return false;
//        }
//
//        rel->symbol_name = reloc->symbol;
//    }
//
//    rel->offset = reloc->offset;
//    rel->type = reloc->type;
//    rel->addend = reloc->addend;
//
//    return true;
//}


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
    int status = loader->scan_file(&obj->loader_data, data, size, &obj->arch);
    if (status != 0) {
        // Loader wasn't happy with the file
        log_error("Invalid file format or corrupt file");
        objfile_put(obj);
        log_ctx_pop();
        return EINVAL;
    }

    obj->loader = loader;

    // Read section metadata
    status = loader->extract_sections(obj->loader_data, _add_section, obj);
    if (status != 0) {
        log_error("Unable to parse sections");
        objfile_put(obj);
        log_ctx_pop();
        return EINVAL;
    }

//    // Read relocations
//    status = loader->extract_relocations(obj->loader_data, _add_relocation, obj);
//    if (status != 0) {
//        log_error("Unable to parse and extract relocations");
//        objfile_put(obj);
//        log_ctx_pop();
//        return EINVAL;
//    }

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

    struct symbol_info info = {
        .name = sym->name,
        .is_reference = sect == NULL && sym->relative,
        .global = sym->binding != SYMBOL_LOCAL,
        .weak = sym->binding == SYMBOL_WEAK,
        .type = sym->type,
        .relative = sym->relative,
        .section = sym->relative ? sect : NULL,
        .offset = sym->relative ? sym->offset : 0,
    };

    bool _continue = cb->cb(cb->cb_data, objfile, &info);
    if (!_continue) {
        cb->status = ECANCELED;
    }

    return _continue;
}


int objfile_extract_symbols(struct objfile *objfile, 
                            bool (*callback)(void*, struct objfile*, const struct symbol_info*),
                            void *callback_data)
{
    assert(objfile->loader != NULL && objfile->loader->extract_symbols != NULL);

    log_ctx_push(LOG_CTX_FILE(objfile->loader->name, objfile->name));

    struct objfile_callback_data cbdata = {
        .status = 0,
        .objfile = objfile,
        .cb = callback,
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
