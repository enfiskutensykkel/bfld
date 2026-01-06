#include "logging.h"
#include "objfile.h"
#include "mfile.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>


struct objfile * objfile_get(struct objfile *objfile)
{
    assert(objfile != NULL);
    assert(objfile->refcnt > 0);
    ++(objfile->refcnt);
    return objfile;
}


void objfile_put(struct objfile *objfile)
{
    assert(objfile != NULL);
    assert(objfile->refcnt > 0);
    
    if (--(objfile->refcnt) == 0) {
        mfile_put(objfile->file);
        if (objfile->name != NULL) {
            free(objfile->name);
        }
        free(objfile);
    }
}


struct objfile * objfile_alloc(struct mfile *file, const char *name,
                               const uint8_t *file_data, size_t file_size)
{
    if (file_data == NULL) {
        file_data = (const uint8_t*) file->data;
        file_size = file->size;
    }

    if (file_data < ((const uint8_t*) file->data) || 
            (file_data + file_size) > (((const uint8_t*) file->data) + file->size)) {
        log_fatal("File data content is outside valid range");
        return NULL;
    }

    struct objfile *objfile = malloc(sizeof(struct objfile));
    if (objfile == NULL) {
        log_fatal("Unable to allocate file handle");
        return NULL;
    }

    if (name != NULL) {
        objfile->name = strdup(name);
    }
    objfile->file = mfile_get(file);
    objfile->refcnt = 1;
    objfile->file_data = file_data;
    objfile->file_size = file_size;
    return objfile;
}
