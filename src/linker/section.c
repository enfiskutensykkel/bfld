#include "logging.h"
#include "section.h"
#include "objectfile.h"
#include "symbol.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>


struct section * section_alloc(struct objectfile *objfile,
                               const char *name,
                               enum section_type type,
                               const uint8_t *content,
                               uint64_t size)
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

    switch (type) {
        case SECTION_LOADER:
        case SECTION_UNWIND:
        case SECTION_CODE:
        case SECTION_THUNK:
        case SECTION_READONLY:
        case SECTION_DATA:
        case SECTION_ZERO:
        case SECTION_METADATA:
        case SECTION_DEBUG:
            break;
        default:
            log_error("Section has unknown type 0x%x", type);
            return NULL;
    }

    if (name == NULL) {
        // Section has no name, let's invent one
        name = section_type_to_string(type);
        log_warning("Section has unknown name. defaulting to '%s'", name);
    }

    struct section *sect = malloc(sizeof(struct section));
    if (sect == NULL) {
        return NULL;
    }

    sect->name = malloc(strlen(name) + 1);
    if (sect->name == NULL) {
        free(sect);
        return NULL;
    }
    strcpy(sect->name, name);

    sect->objfile = objfile != NULL ? objectfile_get(objfile) : NULL;
    sect->refcnt = 1;
    sect->align = 0;
    sect->type = type;
    sect->content = content;
    sect->size = size;
    sect->nrelocs = 0;
    list_head_init(&sect->relocs);
    sect->is_alive = false;

    sect->symbols = NULL;
    sect->nsymbols = 0;
    
    return sect;
}


struct section * section_clone(const struct section *original, const char *name)
{
    struct section *sect = malloc(sizeof(struct section));
    if (sect == NULL) {
        return NULL;
    }

    if (name == NULL) {
        log_debug("Cloning section with original name");
        name = original->name;
    }

    sect->name = malloc(strlen(name) + 1);
    if (sect->name == NULL) {
        free(sect);
        return NULL;
    }
    strcpy(sect->name, name);

    sect->objfile = (original->objfile != NULL ? 
                     objectfile_get(original->objfile) : 
                     NULL);
    sect->refcnt = 1;
    sect->align = original->align;
    sect->type = original->type;
    sect->content = original->content;
    sect->size = original->size;
    sect->nrelocs = 0;
    sect->is_alive = false;
    sect->nsymbols = 0;
    sect->symbols = NULL;

    // Copy relocations from the original
    list_head_init(&sect->relocs);
    list_for_each_entry(reloc, &original->relocs, struct reloc, list_entry) {
        section_add_reloc(sect, reloc->offset, reloc->symbol, 
                          reloc->type, reloc->addend);
    }

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
            objectfile_put(sect->objfile);
        }
        section_clear_relocs(sect);

        if (sect->name != NULL) {
            free(sect->name);
        }

        if (sect->symbols != NULL) {
            free(sect->symbols);
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


bool section_add_symbol_reference(struct section *sect, struct symbol *sym)
{
    size_t low = 0;
    
    if (sect->nsymbols > 0) {
        size_t high = sect->nsymbols;

        while (low < high) {
            size_t mid = low + ((high - low) >> 1);
            
            if (sect->symbols[mid] == sym) {
                log_warning("Reference to symbol '%s' was already added", sym->name);
                return true;
            } else if (sect->symbols[mid] < sym) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
    }

    struct symbol **syms = realloc(sect->symbols, sizeof(struct symbol*) * (sect->nsymbols + 1));
    if (syms == NULL) {
        return false;
    }

    if (low < sect->nsymbols) {
        memmove(&syms[low + 1], 
                &syms[low], 
                (sect->nsymbols - low) * sizeof(struct symbol*));
    }
    syms[low] = sym;  // weak reference

    sect->symbols = syms;
    sect->nsymbols++;
    return true;
}


void section_remove_symbol_reference(struct section *sect, const struct symbol *sym)
{
    if (sect->nsymbols > 0) {
        size_t low = 0;
        size_t high = sect->nsymbols - 1;

        while (low <= high) {
            size_t mid = low + ((high - low) >> 1);
            
            if (sect->symbols[mid] == sym) {
                for (size_t i = mid; i < sect->nsymbols - 1; ++i) {
                    sect->symbols[i] = sect->symbols[i + 1];
                }
                sect->nsymbols--;
                return;
            } else if (sect->symbols[mid] < sym) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
    }
}
