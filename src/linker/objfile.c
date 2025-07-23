#include "objfile.h"
#include "mfile.h"
#include <utils/list.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>


void objfile_put(struct objfile *objfile)
{
    if (--(objfile->refcnt) == 0) {
        list_remove(&objfile->entry);
        mfile_put(objfile->file);
        free(objfile);
    }
}


void objfile_get(struct objfile *objfile)
{
    ++(objfile->refcnt);
}


struct objfile * objfile_alloc(mfile *file, const void *start, 
                               size_t size, const char *name)
{
    if (start == NULL) {
        start = file->data;
        size = file->size;
    }

    if (name == NULL) {
        name = file->name;
    }

    const char *_start = start;
    const char *_end = _start + size;

    if (_start < (const char*) file->data 
            || _end > (((const char*) file->data) + file->size)) {
        return NULL;
    }

    struct objfile *obj = malloc(sizeof(struct objfile) + strlen(name) + 1);
    if (obj == NULL) {
        return NULL;
    }

    mfile_get(file);
    obj->file = file;
    obj->refcnt = 1;
    obj->file_data = start;
    obj->file_size = size;
    list_head_init(&obj->entry);
    strcpy(obj->name, name);

    return obj;
}

