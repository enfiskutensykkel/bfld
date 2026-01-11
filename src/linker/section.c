#include "logging.h"
#include "section.h"
#include "objfile.h"
#include "symbol.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>


struct section * section_alloc(struct objfile *objfile,
                               const char *name,
                               enum section_type type,
                               const uint8_t *content,
                               size_t size)
{
    if (content != NULL) {
        if (objfile == NULL) {
            return NULL;
        }

        if (content < objfile->file_data || content + size > objfile->file_data + objfile->file_size) {
            log_error("Section content is outside valid range");
            return NULL;
        }
    }

    struct section *sect = malloc(sizeof(struct section));
    if (sect == NULL) {
        return NULL;
    }

    const char *filename = objfile != NULL ? objfile->name : "";

    sect->name = malloc(strlen(filename) + 1 + strlen(name) + 1);
    if (sect->name == NULL) {
        free(sect);
        return NULL;
    }
    sprintf(sect->name, "%s:%s", filename, name);

    if (objfile != NULL) {
        sect->objfile = objfile_get(objfile);
    } else {
        sect->objfile = NULL;
    }
    sect->refcnt = 1;
    sect->align = 0;
    sect->type = type;
    sect->content = content;
    sect->size = size;
    sect->nrelocs = 0;
    list_head_init(&sect->relocs);
    sect->is_alive = false;
    
    return sect;
}


struct section * section_get(struct section *sect)
{
    assert(sect != NULL);
    assert(sect->refcnt > 0);
    ++(sect->refcnt);
    return sect;
}


void section_put(struct section *sect)
{
    assert(sect != NULL);
    assert(sect->refcnt > 0);

    if (--(sect->refcnt) == 0) {
        if (sect->objfile != NULL) {
            objfile_put(sect->objfile);
        }
        section_clear_relocs(sect);

        if (sect->name != NULL) {
            free(sect->name);
        }
        free(sect);
    }
}


void section_remove_reloc(struct reloc *reloc)
{
    list_remove(&reloc->list_entry);
    --(reloc->section->nrelocs);
    symbol_put(reloc->symbol);
    free(reloc);
}


struct reloc * section_add_reloc(struct section *section,
                                 uint64_t offset,
                                 struct symbol *symbol,
                                 uint32_t type,
                                 int64_t addend)
{
    struct reloc *reloc = malloc(sizeof(struct reloc));
    if (reloc == NULL) {
        return NULL;
    }

    reloc->section = section;
    reloc->offset = offset;
    reloc->symbol = symbol_get(symbol);
    reloc->type = type;
    reloc->addend = addend;

    list_insert_tail(&section->relocs, &reloc->list_entry);
    ++(section->nrelocs);

    return reloc;
}


void section_clear_relocs(struct section *sect)
{
    while (!list_empty(&sect->relocs)) {
        section_remove_reloc(list_first_entry(&sect->relocs, struct reloc, list_entry));
    }
}
