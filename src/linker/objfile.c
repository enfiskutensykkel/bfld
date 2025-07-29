#include "logging.h"
#include "symtypes.h"
#include "objfilesym.h"
#include "objfile.h"
#include "objfile_loader.h"
#include "mfile.h"
#include "utils/list.h"
#include "utils/rbtree.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "pluginregistry.h"


/*
 * Helper struct to pass as user-data when invoking 
 * callbacks on the objfile_loader.
 */
struct objfile_callback_data
{
    int status;
    struct objfile *objfile;
    bool (*emit_symbol_cb)(void*, struct objfile*, const struct objfile_symbol*);
    void *user_data;
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
        mfile_put(objfile->file);
        free(objfile->name);
        free(objfile);
    }
}


void objfile_get(struct objfile *objfile)
{
    ++(objfile->refcnt);
}


int objfile_init(struct objfile **objfile, mfile *file, const char *name)
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

    mfile_get(file);
    obj->file = file;
    obj->refcnt = 1;
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
        return NULL;
    }

    struct objfile *objfile = NULL;
    int status = objfile_init(&objfile, file, file->name);
    if (status != 0) {
        return NULL;
    }

    log_ctx_push(LOG_CTX_FILE(loader->name, file->name));

    status = loader->parse_file(&objfile->loader_data, file->data, file->size);
    if (status != 0) {
        // Loader wasn't happy with the file
        log_error("Invalid file format or corrupt file");
        objfile_put(objfile);
        log_ctx_pop();
        return NULL;
    }

    objfile->loader = loader;

    log_ctx_pop();
    return objfile;
}



static bool _emit_symbol(void *cb_data, const struct objfile_symbol *sym)
{
    struct objfile_callback_data *cb = cb_data;

    if (cb->emit_symbol_cb == NULL) {
        cb->status = ENOTRECOVERABLE;
        return false;
    }

    bool _continue = cb->emit_symbol_cb(cb->user_data, cb->objfile, sym);
    if (!_continue) {
        cb->status = ECANCELED;
    }

    return _continue;
}


int objfile_extract_symbols(struct objfile* objfile, bool (*callback)(void *user, struct objfile*, const struct objfile_symbol*), void *user)
{
    if (objfile->loader == NULL || objfile->loader->extract_symbols == NULL) {
        log_error("Missing loader for object file");
        return EBADF;
    }
    log_ctx_push(LOG_CTX_FILE(objfile->loader->name, objfile->name));

    struct objfile_callback_data cbdata = {
        .status = 0,
        .objfile = objfile,
        .emit_symbol_cb = callback,
        .user_data = user
    };

    int status = objfile->loader->extract_symbols(objfile->loader_data, 
                                                  _emit_symbol,
                                                  &cbdata);
    if (cbdata.status != 0) {
        log_ctx_pop();
        return cbdata.status;
    }

    if (status != 0) {
        log_error("Could not extract symbols");
        log_ctx_pop();
        return EBADF;
    }

    log_ctx_pop();
    return 0;
}
