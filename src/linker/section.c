#include "logging.h"
#include "section.h"
#include "objfile.h"
#include "symbol.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "march.h"


struct section * section_alloc(struct objfile *objfile,
                               uint64_t idx,
                               const char *name,
                               size_t offset,
                               enum section_type type,
                               const uint8_t *content,
                               size_t size)
{
    struct section *sect = NULL;

    log_ctx_push(LOG_CTX_SECTION(name));
    
    if (size > 0 && content == NULL) {
        log_error("Section size is non-zero but section content is not set");
        goto unwind;
    }

    if (size > 0 && offset == 0) {
        // While technically not impossible, it is strange and unbelievable
        log_error("Section size is non-zero but file offset is not set");
        goto unwind;
    }

    if (offset + size > objfile->file_size) {
        log_error("Section offset is outside valid range");
        goto unwind;
    }

    if (content != NULL) {
        if (content < objfile->file_data || content + size > objfile->file_data + objfile->file_size) {
            log_error("Section content is outside valid range");
            goto unwind;
        }
    }

    if (type == SECTION_ZERO && size > 0) {
        log_fatal("Unexpected section content");
        goto unwind;
    }

    sect = malloc(sizeof(struct section));
    if (sect == NULL) {
        goto unwind;
    }

    sect->name = strdup(name);
    if (sect->name == NULL) {
        free(sect);
        sect = NULL;
        goto unwind;
    }

    sect->objfile = objfile_get(objfile);
    sect->idx = idx;
    sect->offset = offset;
    sect->march = ARCH_UNKNOWN;
    sect->refcnt = 1;
    sect->align = 0;
    sect->type = type;
    sect->content = content;
    sect->size = size;
    sect->nrelocs = 0;
    list_head_init(&sect->relocs);

unwind:
    log_ctx_pop();
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
        objfile_put(sect->objfile);
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
