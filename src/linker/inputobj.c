#include "inputobj.h"
#include <utils/list.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>


void input_objfile_free(struct input_objfile *objfile)
{
    list_remove_entry(&objfile->entry);

    list_for_each_node(sect, &objfile->sects, struct input_sect, entry) {
        list_remove_entry(&sect->entry);
        free(sect);
    }

    free(objfile);
}


struct input_objfile * input_objfile_alloc(const void *content, size_t size)
{
    struct input_objfile *obj = malloc(sizeof(struct input_objfile));
    if (obj == NULL) {
        fprintf(stderr, "Could not allocate memory: %s\n", strerror(errno));
        return NULL;
    }

    obj->content = content;
    obj->size = size;
    list_head_init(&obj->entry);
    list_head_init(&obj->sects);

    return obj;
}


void input_objfile_put_all(struct list_head *objfiles)
{
    list_for_each_node(objfile, objfiles, struct input_objfile, entry) {
        input_objfile_free(objfile);
    }
}


struct input_sect * input_sect_alloc(struct input_objfile *objfile,
                                     uint32_t idx,
                                     uint64_t offset,
                                     const void *content,
                                     size_t size)
{
    struct input_sect *sect = malloc(sizeof(struct input_sect));
    if (sect == NULL) {
        fprintf(stderr, "Could not allocate memory: %s\n", strerror(errno));
        return NULL;
    }

    sect->objfile = objfile;
    sect->idx = idx;
    sect->offset = offset;
    sect->content = content;
    sect->size = size;

    list_append_entry(&objfile->sects, &sect->entry);
    return sect;
}

