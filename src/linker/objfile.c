#include "logging.h"
#include "objfile.h"
#include "objfile_loader.h"
#include "mfile.h"
#include "symbol.h"
#include "utils/list.h"
#include "utils/rbtree.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "pluginregistry.h"


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
    list_for_each_entry(loader_entry, &objfile_loaders, struct plugin_registry_entry, list_node) {
        const struct objfile_loader *loader = loader_entry->plugin;
        if (loader->probe(data, size)) {
            return loader;
        }
    }

    return NULL;
}


void objfile_put(struct objfile *objfile)
{
    if (--(objfile->refcnt) == 0) {
        if (objfile->loader != NULL && objfile->loader->release != NULL) {
            objfile->loader->release(objfile->loader_data);
        }
        free(objfile->name);
        mfile_put(objfile->file);
        free(objfile);
    }
}


void objfile_get(struct objfile *objfile)
{
    ++(objfile->refcnt);
}


int objfile_init(struct objfile **objfile, const char *name,
                 const uint8_t *data, size_t size)
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
    obj->data = data;
    obj->size = size;
    obj->loader_data = NULL;
    obj->loader = NULL;

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
        log_error("Unrecognized format");
        return NULL;
    }

    log_ctx_push(LOG_CTX_FILE(loader->name, file->name));

    mfile_get(file);

    void *loader_data = NULL;
    int status = loader->parse_file(&loader_data, file->name,
                                    file->data, file->size);
    if (status != 0) {
        // Loader wasn't happy with the file
        mfile_put(file);
        log_error("Corrupt file");
        log_ctx_pop();
        return NULL;
    }

    struct objfile *objfile = NULL;
    status = objfile_init(&objfile, file->name, file->data, file->size);
    if (status != 0) {
        mfile_put(file);
        if (loader->release != NULL) {
            loader->release(loader_data);
        }
        log_ctx_pop();
        return NULL;
    }

    objfile->file = file;
    objfile->loader_data = loader_data;
    objfile->loader = loader;

    log_ctx_pop();
    return objfile;
}


/*
 * Helper struct to pass as user-data to the object file loader's 
 * extract_symbols
 */
struct symbol_data
{
    int status;
    struct objfile *objfile;
    struct rb_tree *global_symtab;
    struct rb_tree *local_symtab;
};


static int insert_emitted_symbols(void *user_data, const struct objfile_symbol *objsym)
{
    int status;
    struct symbol *sym = NULL;
    struct symbol_data *ctx = (struct symbol_data*) user_data;
    
    if (objsym->bind == SYMBOL_LOCAL) {
        status = symbol_create(&sym, ctx->objfile, objsym->name, 
                                objsym->bind, objsym->type, ctx->local_symtab);
    } else {
        status = symbol_create(&sym, ctx->objfile, objsym->name, 
                               objsym->bind, objsym->type, ctx->global_symtab);
    }

    
    if (status == EEXIST && sym->binding != SYMBOL_WEAK) {
        ctx->status = EEXIST;
        log_error("Multiple definitions for symbol '%s'; defined in both %s and %s", 
                  sym->name, ctx->objfile->name, sym->source->name);
        return -1;

    } else if (status != 0) {
        ctx->status = ENOMEM;
        return -1;
    }

    if (objsym->defined) {
        status = symbol_resolve_definition(sym, ctx->objfile, 
                                           objsym->sect_idx, objsym->offset, objsym->size);
        if (status != 0) {
            ctx->status = EEXIST;
            log_error("Multiple definitions for symbol '%s'; defined in both %s and %s",
                      sym->name, ctx->objfile->name, sym->definition->name);
            return -1;
        }
    }

    return 0;
}


int objfile_extract_symbols(struct objfile* objfile,
                            struct rb_tree *global_symtab,
                            struct rb_tree *local_symtab)
{
    if (objfile->loader == NULL || objfile->loader->extract_symbols == NULL) {
        log_fatal("Missing loader for object file");
        return EBADF;
    }
    log_ctx_push(LOG_CTX_FILE(objfile->loader->name, objfile->name));

    struct symbol_data data = {
        .status = 0,
        .objfile = objfile,
        .global_symtab = global_symtab,
        .local_symtab = local_symtab
    };

    objfile->loader->extract_symbols(objfile->loader_data, 
                                     insert_emitted_symbols,
                                     &data);

    log_ctx_pop();
    return data.status;
}
