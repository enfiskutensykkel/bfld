#include "objfile.h"
#include "objfile_loader.h"
#include "mfile.h"
#include "symbol.h"
#include "utils/list.h"
#include "utils/rbtree.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>


struct objfile
{
    char *filename;
    int refcnt;
    mfile *file;
    const uint8_t *file_data;
    size_t file_size;
    void *loader_data;
    const struct objfile_loader *loader;
};


/*
 * Maintain a list of object file loaders.
 */ 
static struct list_head loaders = LIST_HEAD_INIT(loaders);


/*
 * Definition of a object file loader.
 */
struct objfile_loader
{
    int refcnt;
    char *name;
    struct list_head list_node;
    const struct objfile_loader_ops *ops;
};


struct objfile_loader * objfile_loader_register(const char *name, 
        const struct objfile_loader_ops *ops)
{
    struct objfile_loader *loader = malloc(sizeof(struct objfile_loader));
    if (loader == NULL) {
        return NULL;
    }

    loader->name = strdup(name);
    if (loader->name == NULL) {
        free(loader);
        return NULL;
    }

    loader->refcnt = 1;
    loader->ops = ops;
    list_insert_tail(&loaders, &loader->list_node);
    return loader;
}


static void objfile_loader_get(struct objfile_loader *loader)
{
    ++(loader->refcnt);
}


static void objfile_loader_put(struct objfile_loader *loader)
{
    if (--(loader->refcnt) == 0) {
        list_remove(&loader->list_node);
        free(loader->name);
        free(loader);
    }
}


void objfile_loader_unregister(struct objfile_loader *loader)
{
    list_remove(&loader->list_node);
}


__attribute__((destructor(65535)))
static void cleanup_loaders(void)
{
    list_for_each_entry(loader, &loaders, struct objfile_loader, list_node) {
        objfile_loader_put(loader);
    }
}


const struct objfile_loader * objfile_loader_probe(const uint8_t *data, size_t size)
{
    list_for_each_entry(loader, &loaders, struct objfile_loader, list_node) {
        if (loader->ops->probe != NULL) {
            if (loader->ops->probe(data, size)) {
                return loader;
            }
        }
    }

    return NULL;
}


const char * objfile_loader_name(const struct objfile_loader *loader)
{
    if (loader != NULL) {
        return loader->name;
    }

    return NULL;
}


const struct objfile_loader * objfile_loader_find(const char *name)
{
    list_for_each_entry(loader, &loaders, struct objfile_loader, list_node) {
        if (strcmp(loader->name, name) == 0) {
            return loader;
        }
    }

    return NULL;
}


void objfile_put(struct objfile *objfile)
{
    if (--(objfile->refcnt) == 0) {
        if (objfile->loader != NULL && objfile->loader->ops->release != NULL) {
            objfile->loader->ops->release(objfile->loader_data);
        }
        free(objfile->filename);
        mfile_put(objfile->file);
        if (objfile->loader != NULL) {
            objfile_loader_put((struct objfile_loader*) objfile->loader);
        }
        free(objfile);
    }
}


void objfile_get(struct objfile *objfile)
{
    ++(objfile->refcnt);
}


const char * objfile_filename(const struct objfile *objfile)
{
    return objfile->filename;
}


const struct objfile_loader * objfile_get_loader(const struct objfile *objfile)
{
    return objfile->loader;
}


int objfile_init(struct objfile **objfile, const char *filename,
                 const uint8_t *data, size_t size)
{
        
    struct objfile *obj = malloc(sizeof(struct objfile));
   if (obj == NULL) {
        return ENOMEM;
    }

    obj->filename = strdup(filename);
    if (obj->filename == NULL) {
        free(obj);
        return ENOMEM;
    }

    obj->file = NULL;
    obj->refcnt = 1;
    obj->file_data = data;
    obj->file_size = size;
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

    mfile_get(file);

    void *loader_data = NULL;
    int status = loader->ops->parse_file(&loader_data, file->name,
                                         file->data, file->size);
    if (status != 0) {
        // Loader wasn't happy with the file
        mfile_put(file);
        return NULL;
    }

    struct objfile *objfile = NULL;
    status = objfile_init(&objfile, file->name, file->data, file->size);
    if (status != 0) {
        mfile_put(file);
        if (loader->ops->release != NULL) {
            loader->ops->release(loader_data);
        }
        return NULL;
    }

    objfile->file = file;
    objfile->loader_data = loader_data;
    objfile->loader = loader;
    objfile_loader_get((struct objfile_loader*) loader);

    return objfile;
}


/*
 * Helper struct to pass as user-data to the object file loader's 
 * extract_symbols
 */
struct symbol_data
{
    int status;
    const struct objfile *objfile;
    struct rb_tree *global_symtab;
    struct rb_tree *local_symtab;
};


static int insert_emitted_symbols(void *user_data, const struct objfile_symbol *objsym)
{
    int status;
    struct symbol *sym = NULL;
    struct symbol_data *ctx = (struct symbol_data*) user_data;
    
    if (objsym->bind == SYMBOL_LOCAL) {
        status = symbol_create(&sym, objsym->name, objsym->bind, objsym->type,
                               ctx->local_symtab);
    } else {
        status = symbol_create(&sym, objsym->name, objsym->bind, objsym->type,
                               ctx->global_symtab);
    }

    // TODO use sym to say something about where the symbol came from
    if (status == EEXIST) {
        fprintf(stderr, "%s: Multiple definitions of symbol %s\n", 
                        ctx->objfile->filename, objsym->name);
        return -1;
    }

    if (objsym->defined) {
        status = symbol_resolve_definition(sym, (struct objfile*) ctx->objfile, 
                                           objsym->sect_idx, objsym->offset, objsym->size);
    }

    return 0;
}


int objfile_extract_symbols(const struct objfile* objfile,
                            struct rb_tree *global_symtab,
                            struct rb_tree *local_symtab)
{
    if (objfile->loader == NULL || objfile->loader->ops->extract_symbols == NULL) {
        return EBADF;
    }

    struct symbol_data data = {
        .status = 0,
        .objfile = objfile,
        .global_symtab = global_symtab,
        .local_symtab = local_symtab
    };

    objfile->loader->ops->extract_symbols(objfile->loader_data, 
                                          insert_emitted_symbols,
                                          &data);

    return data.status;
}
