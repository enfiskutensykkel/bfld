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


int symbol_bind_definition(struct symbol *sym,
                           struct section *section,
                           uint64_t offset,
                           uint64_t size)
{
    assert(sym != NULL);    

    // Check that we are not overwriting a previous strong definition
    bool strong = sym->binding != SYMBOL_WEAK && !sym->is_common;
    if (symbol_is_defined(sym) && !strong) {
        if (sym->is_absolute) {
            log_error("Redefinition for symbol '%s', was already defined at address 0x%llx", 
                    sym->name, sym->value);
        } else {
            log_error("Redefinition for symbol '%s', was already defined in %s:%s", sym->name,
                    sym->section->objfile->name, sym->section->name);
        }
        return EALREADY;
    }

    // We already defined so release previous reference
    if (sym->section != NULL) {
        log_debug("Replacing previous weak definition for symbol '%s'", sym->name);
        section_put(sym->section);
        sym->section = NULL;
    }

    if (section == NULL) {
        // This is an absolute definition
        sym->section = NULL;
        sym->is_absolute = true;
        sym->value = offset;
        sym->offset = 0;
        sym->size = size;
        log_trace("Symbol '%s' is defined at address 0x%lx", 
                sym->name, sym->value);

    } else {
        // This is a relative definition
        sym->is_absolute = false;
        sym->value = 0;
        sym->offset = offset;
        sym->section = section_get(section);
        sym->size = size;
        log_trace("Symbol '%s' is defined in %s:%s", sym->name,
                sym->section->objfile->name, sym->section->name);
    }

    return 0;
}


int symbol_resolve_definition(struct symbol *existing, const struct symbol *incoming)
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

    // For common symbols, adjust size and alignment
    if (existing->is_common && incoming->is_common) {
        if (incoming->align > existing->align) {
            existing->align = incoming->align;
        }

        if (incoming->size > existing->size) {
            existing->size = incoming->size;
        }
    }

    // Incoming is an undefined symbol reference 
    if (!symbol_is_defined(incoming)) {
        return 0;
    }

    // Incoming definition is weak, keep existing
    if (incoming->binding == SYMBOL_WEAK) {
        return 0;
    }

    // Existing definition is strong (and incoming is strong)
    bool strong = existing->binding != SYMBOL_WEAK && !existing->is_common;
    if (strong && symbol_is_defined(existing)) {
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

    int status = symbol_bind_definition(existing, incoming->section, 
                                        incoming->offset, incoming->size);
    if (status != 0) {
        return EEXIST;
    }

    existing->binding = incoming->binding;
    existing->type = incoming->type;

    if (!incoming->is_common) {
        existing->is_common = false;
    }

    return 0;
}
