#include "logging.h"
#include "symtypes.h"
#include "section.h"
#include "objfile.h"
#include "objfile_loader.h"
#include "mfile.h"
#include "utils/list.h"
#include "utils/rbtree.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>


#define COMMON_SECTION_KEY  0xdeadbeef


/*
 * Helper struct to pass as user-data when invoking 
 * callbacks on the objfile_loader.
 */
struct symbol_callback_data
{
    int status;
    struct objfile *objfile;
    bool (*cb)(void*, struct objfile*, const struct symbol_info*);
    void *cb_data;
};


/*
 * Helper struct to pass as user-data when invoking 
 * callbacks on the objfile_loader.
 */
struct reloc_callback_data
{
    int status;
    struct objfile *objfile;
    bool (*cb)(void*, struct objfile*, const struct reloc_info*);
    void *cb_data;
};


/*
 * Internal type to allow lookups of sections on object files.
 */
struct isection
{
    struct rb_node keymap;          // look up section from its key
    struct rb_node namemap;         // look up section from its name
    uint64_t sect_key;              // copy of the sect_key
    char *name;                     // name of the section
    size_t offset;                  // offset from file start (if applicable)
    enum section_type type;         // section type
    uint64_t align;                 // base address alignment requirements
    size_t size;                    // size of the section
    const uint8_t *content;         // section content pointer
};


/*
 * Entry in the object file loader list.
 */
struct loader_entry
{
    struct list_head node;
    const struct objfile_loader *loader;
};


static struct list_head loaders = LIST_HEAD_INIT(loaders);


/*
 * Remove all registered loaders.
 */
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
            || loader->extract_relocs == NULL) {
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


static void remove_section(struct objfile *objfile, struct isection *sect)
{
    assert(objfile != NULL);

    rb_remove(&objfile->sections, &sect->keymap);
    rb_remove(&objfile->sects_by_name, &sect->namemap);
    free(sect);
    --(objfile->num_sections);
}


static struct isection * create_section(struct objfile *objfile, 
                                        uint64_t sect_key, const char *name)
{
    struct rb_node **pos = &(objfile->sections.root), *parent = NULL;

    while (*pos != NULL) {
        struct isection *this = rb_entry(*pos, struct isection, keymap);

        parent = *pos;
        if (sect_key < this->sect_key) {
            pos = &((*pos)->left);
        } else if (sect_key > this->sect_key) {
            pos = &((*pos)->right);
        } else {
            // Section with the same key already exist
            log_error("Section %u is already loaded", sect_key);
            return NULL;
        }
    }

    struct isection *sect = malloc(sizeof(struct isection));
    if (sect == NULL) {
        return NULL;
    }

    sect->name = strdup(name);
    if (sect->name == NULL) {
        free(sect);
        return NULL;
    }

    sect->sect_key = sect_key;
    sec->offset = 0;
    sect->type = SECTION_ZERO;
    sect->align = 0;
    sect->size = 0;
    sect->content = NULL;

    rb_insert_node(&sect->keymap, parent, pos);
    rb_insert_fixup(&objfile->sections, &sect->keymap);
    rb_node_init(&sect->namemap);

    ++(objfile->num_sections);
    return map->section;
}


void objfile_put(struct objfile *objfile)
{
    assert(objfile != NULL);
    assert(objfile->refcnt > 0);

    if (--(objfile->refcnt) == 0) {
        while (objfile->sections.root != NULL) {
            struct isection *sect = rb_entry(objfile->sections.root, struct isection, keymap);
            remove_section(objfile, sect);
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
    assert(objfile != NULL);
    assert(objfile->refcnt > 0);
    ++(objfile->refcnt);
}


static bool add_section(void *ctx, const struct objfile_section *sect)
{
    struct isection *s = create_section(ctx, sect->section, sect->name);
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

    obj->file = NULL;
    obj->refcnt = 1;

    obj->loader = NULL;
    obj->loader_data = NULL;
    rb_tree_init(&obj->sections);

    struct section *common = create_section(obj, 0xdeadbeef, ".common");
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
    status = loader->extract_sections(obj->loader_data, add_section, obj);
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


struct section * objfile_get_section(struct objfile *objfile, uint64_t sect_key)
{
    
    if (sect_key == 0) {
        return NULL;
    }

    struct rb_node *node = objfile->sections.root;

    while (node != NULL) {
        struct isection *sect = rb_entry(node, struct isection, keymap);

        if (sect_key < sect->sect_key) {
            node = node->left;
        } else if (sect_key > sect->sect_key) {
            node = node->right;
        } else {
            struct section *s = section_init(objfile, sect->sect_key,
                                             sect->name);
            if (s == NULL) {
                return NULL;
            }
        }
    }

    return NULL;
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


static bool emit_relocation(void *cb_data, const struct objfile_relocation *rel)
{
    struct reloc_callback_data *cb = cb_data;
    struct objfile *objfile = cb->objfile;

    struct reloc_info info = {
        .section = find_section(objfile, rel->section),
        .symbol_name = NULL,
        .target = NULL,
        .offset = rel->offset,
        .type = rel->type,
        .addend = rel->addend,
    };

    if (info.section == NULL) {
        log_error("Invalid section in relocation");
        return false;
    }

    if (rel->commonref) {
        info.target = find_section(objfile, 0xdeadbeef);

    } else if (rel->sectionref != 0) {
        info.target = find_section(objfile, rel->sectionref);

        if (info.target == NULL) {
            cb->status = EBADF;
            log_error("Invalid target section in relocation");
            return false;
        }
    } else {
        info.symbol_name = rel->symbol;
    }

    bool _continue = cb->cb(cb->cb_data, objfile, &info);
    if (!_continue) {
        cb->status = ECANCELED;
        return false;
    }

    return true;
}


static bool emit_symbol(void *cb_data, const struct objfile_symbol *sym)
{
    struct symbol_callback_data *cb = cb_data;
    struct objfile *objfile = cb->objfile;

    struct section *sect = NULL;
    if (sym->common) {
        sect = find_section(objfile, 0xdeadbeef);
    } else if (sym->section > 0) {
        sect = find_section(objfile, sym->section);

        if (sect == NULL) {
            cb->status = EBADF;
            log_error("Invalid section reference in symbol '%s'", sym->name);
            return false;
        }
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

    objfile_get(objfile);
    log_ctx_push(LOG_CTX_FILE(objfile->loader->name, objfile->name));

    struct symbol_callback_data cbdata = {
        .status = 0,
        .objfile = objfile,
        .cb = callback,
        .cb_data = callback_data
    };

    int status = objfile->loader->extract_symbols(objfile->loader_data, 
                                                  emit_symbol,
                                                  &cbdata);
    if (cbdata.status != 0) {
        log_ctx_pop();
        objfile_put(objfile);
        return cbdata.status;
    }

    if (status != 0) {
        log_error("Extracting symbols failed");
        log_ctx_pop();
        objfile_put(objfile);
        return EBADF;
    }

    log_ctx_pop();
    objfile_put(objfile);
    return 0;
}


int objfile_extract_relocs(struct objfile *objfile,
                           bool (*callback)(void*, struct objfile*, const struct reloc_info*),
                           void *callback_data)
{
    objfile_get(objfile);
    log_ctx_push(LOG_CTX_FILE(objfile->loader->name, objfile->name));

    struct reloc_callback_data cbdata = {
        .status = 0,
        .objfile = objfile,
        .cb = callback,
        .cb_data = callback_data
    };

    int status = objfile->loader->extract_relocs(objfile->loader_data,
                                                 emit_relocation,
                                                 &cbdata);
    if (cbdata.status != 0) {
        log_ctx_pop();
        objfile_put(objfile);
        return cbdata.status;
    }

    if (status != 0) {
        log_error("Extracting relocations failed");
        log_ctx_pop();
        objfile_put(objfile);
        return EBADF;
    }

    log_ctx_pop();
    objfile_put(objfile);
    return 0;
}
