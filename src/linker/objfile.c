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
    struct objfile_private *loader_data;
    struct objfile_loader *loader;

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

    loader->ops = ops;
    list_insert_tail(&loaders, &loader->list_node);
    return loader;
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


const char * objfile_loader_name(const struct objfile *objfile)
{
    return objfile->loader != NULL ? objfile->loader->name : NULL;
}


const struct objfile_loader * objfile_get_loader(const struct objfile *objfile)
{
    return objfile->loader;
}


static int objfile_alloc(struct objfile **objfile, mfile *file, const char *filename,
                         const uint8_t *start, size_t size)
{
    if (start == NULL) {
        start = file->data;
        size = file->size;
    }

    if (filename == NULL) {
        filename = file->name;
    }

    const uint8_t *_start = start;
    const uint8_t *_end = _start + size;

    if (_start < (const uint8_t*) file->data 
            || _end > (((const uint8_t*) file->data) + file->size)) {
        return EINVAL;
    }

    struct objfile *obj = malloc(sizeof(struct objfile));
    if (obj == NULL) {
        return ENOMEM;
    }

    obj->filename = strdup(filename);
    if (obj->filename == NULL) {
        free(obj->filename);
        return ENOMEM;
    }

    mfile_get(file);
    obj->file = file;
    obj->refcnt = 1;
    obj->file_data = start;
    obj->file_size = size;
    obj->loader_data = NULL;
    obj->loader = NULL;

    *objfile = obj;
    return 0;
}

