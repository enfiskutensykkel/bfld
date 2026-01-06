#include "symbol.h"
#include "section.h"
#include "logging.h"
#include "objfile.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>


struct symbol * symbol_get(struct symbol *sym)
{
    assert(sym != NULL);
    assert(sym->refcnt > 0);
    ++(sym->refcnt);
    return sym;
}


void symbol_put(struct symbol *sym)
{
    assert(sym != NULL);
    assert(sym->refcnt > 0);

    if (--(sym->refcnt) == 0) {
        if (sym->section != NULL) {
            section_put(sym->section);
        }
        free(sym->name);
        free(sym);
    }
}


struct symbol * symbol_alloc(const char *name, enum symbol_type type, 
                             enum symbol_binding binding)
{
    struct symbol *sym = malloc(sizeof(struct symbol));
    if (sym == NULL) {
        return NULL;
    }

    sym->name = strdup(name);
    if (sym->name == NULL) {
        free(sym);
        return NULL;
    }

    sym->value = 0;
    sym->binding = binding;
    sym->type = type;
    sym->refcnt = 1;
    sym->align = 0;
    sym->size = 0;
    sym->is_absolute = false;
    sym->is_common = false;
    sym->section = NULL;
    sym->offset = 0;

    return sym;
}


int symbol_bind_common(struct symbol *sym, uint64_t size, uint64_t align)
{
    assert(sym != NULL);

    if (symbol_is_defined(sym)) {
        log_error("Redefinition of symbol '%s' as common symbol", sym->name);
        return EALREADY;
    }

    if (sym->is_common) {
        log_error("Redefinition of common symbol '%s', symbol was already defined", sym->name);
        return EALREADY;
    }
    
    assert(sym->section == NULL);
    sym->is_absolute = false;
    sym->offset = 0;
    sym->size = size;
    sym->align = align;
    sym->is_common = true;

    return 0;
}


int symbol_bind_definition(struct symbol *sym,
                           struct section *section,
                           uint64_t offset,
                           uint64_t size)
{
    assert(sym != NULL);    

    // Check that we are not overwriting a previous strong definition
    if (symbol_is_defined(sym) && sym->binding != SYMBOL_WEAK && !sym->is_common) {
        if (sym->is_absolute) {
            log_error("Redefinition for symbol '%s', was already defined at address 0x%llx", 
                    sym->name, sym->value);
        } else {
            log_error("Redefinition for symbol '%s', was already defined in %s:%s", sym->name,
                    sym->section->objfile->name, sym->section->name);
        }
        return EALREADY;
    }

    struct section *old_section = sym->section;
    sym->size = size;
    sym->is_common = false;  // symbol is now defined, it can not be common

    if (section == NULL) {
        // This is an absolute definition
        sym->section = NULL;
        sym->is_absolute = true;
        sym->value = offset;
        sym->offset = 0;
        sym->align = 0;  // FIXME: make sure that address is an alignment of align
        log_trace("Symbol '%s' is defined at address 0x%lx", 
                sym->name, sym->value);

    } else {
        // This is a relative definition
        sym->is_absolute = false;
        sym->value = 0;
        sym->offset = offset;
        sym->section = section_get(section);
        log_trace("Symbol '%s' is defined in %s:%s", sym->name,
                sym->section->objfile->name, sym->section->name);
    }

    // Release old section
    if (old_section != NULL) {
        section_put(old_section);
    }

    return 0;
}


int symbol_merge(struct symbol *existing, const struct symbol *incoming)
{
    assert(existing != NULL);

    if (existing->binding == SYMBOL_LOCAL) {
        log_error("Cannot resolve local symbol '%s'", existing->name);
        return EINVAL;
    }
    
    if (incoming == NULL) {
        return 0;
    }

    if (incoming->binding == SYMBOL_LOCAL || strcmp(existing->name, incoming->name) != 0) {
        log_error("Invalid symbol definition for '%s'", existing->name);
        return EINVAL;
    }

    if (existing->is_common && incoming->is_common) {
        if (incoming->align > existing->align) {
            existing->align = incoming->align;
        }

        if (incoming->size > existing->size) {
            existing->size = incoming->size;
        }

        log_trace("Updated alignment and size of common symbol '%s'", existing->name);
        return 0;
    }

    // If incoming is an undefined symbol reference, keep existing definition
    if (!symbol_is_defined(incoming)) {
        return 0;
    }

    if (!existing->is_common) {

        // If incoming definition is weak, keep existing definition
        if (symbol_is_defined(existing) && incoming->binding == SYMBOL_WEAK) {
            return 0;
        }

        // If existing definition is strong and incoming definition is strong,
        // we have a multiple definition error
        if (symbol_is_defined(existing) && existing->binding != SYMBOL_WEAK) {
            if (existing->is_absolute) {
                log_error("Multiple definitions for symbol '%s', previously defined at address 0x%lx",
                        existing->name, existing->value);

            } else {
                log_error("Multiple definitions for symbol '%s', previously defined in %s:%s",
                        existing->name, 
                        existing->section->objfile->name,
                        existing->section->name);
            }
            return EEXIST;
        }
    }

    int status = symbol_bind_definition(existing, incoming->section, 
                                        incoming->offset, incoming->size);
    switch (status) {
        case 0: break;
        case EALREADY:
            return EEXIST;
        default:
            return EINVAL;
    }

    existing->binding = incoming->binding;
    existing->type = incoming->type;

    return 0;
}
