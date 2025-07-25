#include "ar.h"
#include "elf.h"
#include "objfile.h"
#include "utils/list.h"
#include <stddef.h>
#include <errno.h>


int objfile_parse(struct list_head *objfiles, mfile *fp)
{
    if (ar_check_magic(fp->data)) {
        return ar_parse_members(fp, objfiles);

    } else if (elf_check_magic(fp->data)) {
        struct objfile *obj = objfile_alloc(fp, NULL, 0, NULL);
        if (obj == NULL) {
            return ENOMEM;
        }

        int status = elf_load_objfile(obj);
        if (status != 0) {
            objfile_put(obj);
            return status;
        }

        list_insert_tail(objfiles, &obj->entry);
        return 0;
    }

    fprintf(stderr, "%s: Unrecognized file format\n", fp->name);
    return EBADF;
}
