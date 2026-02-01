#include "logging.h"
#include "objectfile.h"
#include "mfile.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "symbol.h"
#include "symbols.h"
#include "section.h"
#include "sections.h"
#include "objectfile_reader.h"


// strdup is a POSIX function
extern char * strdup(const char *s);


struct objectfile * objectfile_get(struct objectfile *objfile)
{
    assert(objfile != NULL);
    assert(objfile->refcnt > 0);
    ++(objfile->refcnt);
    return objfile;
}


void objectfile_put(struct objectfile *objfile)
{
    assert(objfile != NULL);
    assert(objfile->refcnt > 0);
    
    if (--(objfile->refcnt) == 0) {
        mfile_put(objfile->file);
        free(objfile->name);
        free(objfile);
    }
}


struct objectfile * objectfile_alloc(struct mfile *file, 
                                     const char *name,
                                     const uint8_t *file_data, 
                                     size_t file_size)
{
    if (file_data == NULL) {
        file_data = (const uint8_t*) file->data;
        file_size = file->size;
    }

    if (file_data < ((const uint8_t*) file->data) || 
            (file_data + file_size) > (((const uint8_t*) file->data) + file->size)) {
        log_error("Object file data content is outside valid range");
        return NULL;
    }

    if (name == NULL) {
        log_warning("Object file has unknown name");
        name = "UNKNOWN";
    }

    struct objectfile *objfile = malloc(sizeof(struct objectfile));
    if (objfile == NULL) {
        return NULL;
    }

    objfile->name = strdup(name);
    if (objfile->name == NULL) {
        free(objfile);
        return NULL;
    }

    objfile->file = mfile_get(file);
    objfile->refcnt = 1;
    objfile->file_data = file_data;
    objfile->file_size = file_size;
    return objfile;
}


