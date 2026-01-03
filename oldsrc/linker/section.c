#include "secttypes.h"
#include "section.h"
#include "objfile.h"
#include <stdlib.h>
#include <errno.h>
#include "logging.h"
#include <assert.h>


int section_init(struct section **section, struct objfile *objfile, 
                 uint64_t sect_key, const char *name)
{
    assert(objfile != NULL);

    *section = NULL;

    if (sect_key == 0) {
        log_error("Section identifier can not be 0");
        return EINVAL;
    }

    struct section *sect = malloc(sizeof(struct section));
    if (sect == NULL) {
        return ENOMEM;
    }

    sect->name = strdup(name);
    if (sect->name == NULL) {
        free(sect);
        return ENOMEM;
    }

    sect->refcnt = 1;
    sect->sect_key = key;
    sect->objfile = objfile;
    objfile_get(sect->objfile);
    sect->type = SECTION_ZERO;
    sect->content = NULL;
    sect->size = 0;
    sect->align = 0;

    *section = sect;
    return 0;
}


void section_get(struct section *sect)
{
    assert(sect != NULL);
    assert(sect->refcnt > 0);
    ++(sect->refcnt);
}


void section_put(struct section *sect)
{
    assert(sect != NULL);
    assert(sect->refcnt > 0);

    if (--(sect->refcnt) == 0) {
        objfile_put(sect->objfile);
        free(sect->name);
        free(sect);
    }
}
