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
    sym->relative = true;
    sym->binding = binding;
    sym->type = type;
    sym->refcnt = 1;
    sym->align = 0;
    sym->size = 0;
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

    // Check that we are not overwriting a previous definition
    if (sym->binding != SYMBOL_WEAK && symbol_is_defined(sym)) {
        if (symbol_is_absolute(sym)) {
            log_error("Symbol '%s' is already defined at address 0x%llx", 
                    sym->name, sym->value);
        } else {
            log_error("Symbol '%s' is already defined in %s:%s", sym->name,
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
        sym->relative = false;
        sym->value = offset;
        sym->offset = 0;
        sym->size = size;
        log_trace("Symbol '%s' is defined at address 0x%llx", 
                sym->name, sym->value);

    } else {
        // This is a relative definition
        sym->relative = true;
        sym->value = 0;
        sym->offset = offset;
        sym->section = section_get(section);
        sym->size = size;
        log_trace("Symbol '%s' is defined in %s:%s", sym->name,
                sym->section->objfile->name, sym->section->name);
    }

    return 0;
}


static inline void override_symbol(struct symbol *existing, const struct symbol *truth)
{
    assert(!symbol_is_defined(existing) || existing->binding == SYMBOL_WEAK);

    if (!symbol_is_defined(truth)) {
        return;
    }

    int status = symbol_bind_definition(existing, truth->section, truth->offset, truth->size);
    assert(status == 0);
    log_debug("Updated symbol definition");

    if (truth->align > existing->align) {
        existing->align = truth->align;
        log_debug("Adjusted symbol alignment requirements");
    }

    existing->binding = truth->binding;
    existing->type = truth->type;
}


int symbol_resolve_definition(struct symbol *existing, const struct symbol *incoming)
{
    assert(existing != NULL);
    assert(incoming != NULL);
    assert(strcmp(existing->name, incoming->name) == 0);
    assert(existing->binding != SYMBOL_LOCAL);
    assert(incoming->binding != SYMBOL_LOCAL);


    // Both symbols are references (undefined)
    if (!symbol_is_defined(existing) && !symbol_is_defined(incoming)) {
        return 0;
    }

    // Existing is a definition, incoming is a reference
    if (symbol_is_defined(existing) && !symbol_is_defined(incoming)) {
        return 0;
    }

    // Existing is a reference, incoming is a definition
    if (!symbol_is_defined(existing) && symbol_is_defined(incoming)) {
        override_symbol(existing, incoming);
        return 0;
    }

    // Incoming definition is weak, keep the existing
    if (incoming->binding == SYMBOL_WEAK) {
        return 0;
    }

    // Existing definition is weak, keep the incoming
    if (existing->binding == SYMBOL_WEAK) {
        override_symbol(existing, incoming);
        return 0;
    }

    log_error("Multiple definitions for symbol '%s', was previously defined in %s:%s",
            existing->name, 
            existing->section->objfile->name, 
            existing->section->name);

    return EEXIST;
}
