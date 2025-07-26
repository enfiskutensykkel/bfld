#include "objfile.h"
#include "objfile_loader.h"
#include "mfile.h"
#include "utils/list.h"
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
    // list over IR sections?
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


void objfile_loader_get(struct objfile_loader *loader)
{
    ++(loader->refcnt);
}


void objfile_loader_put(struct objfile_loader *loader)
{
    if (--(loader->refcnt) == 0) {
        list_remove(&loader->list_node);
        free(loader->name);
        free(loader);
    }
}


__attribute__((destructor))
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

    return objfile;
}
