#include "symbol.h"
#include "section.h"
#include "logging.h"
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
    sym->section = NULL;
    sym->offset = 0;

    return sym;
}


int symbol_link_definition(struct symbol *sym,
                           struct section *section,
                           uint64_t offset)
{
    if (!(section != NULL || (section == NULL && offset > 0))) {
        return EINVAL;
    }

    if (sym->binding != SYMBOL_WEAK && symbol_is_defined(sym)) {
        // Check that we are not overwriting a previous definition
        return EALREADY;
    }

    if (sym->section != NULL) {
        section_put(sym->section);
        sym->section = NULL;
    }

    if (section != NULL) {
        sym->section = section_get(section);
    }
    sym->offset = offset;
    sym->relative = sym->section != NULL;
    return 0;
}


static void override_symbol(struct symbol *victim, struct symbol *truth)
{
    assert(strcmp(victim->name, truth->name) == 0);
    assert(!symbol_is_defined(victim) || victim->binding == SYMBOL_WEAK);
    assert(symbol_is_defined(truth));

    int status = symbol_link_definition(victim, truth->section, truth->offset);
    assert(status == 0);

    // Choose the highest alignment for both
    if (victim->align > truth->align) {
        truth->align = victim->align;
        log_warning("Adjusting alignment for symbol '%s' to %llu",
                truth->name, truth->align);
    }
    victim->align = truth->align;
    victim->binding = truth->binding;
    victim->type = truth->type;
    victim->value = truth->value;
    log_trace("Linked symbol reference '%s' to symbol definition",
            truth->name);
}


int symbol_resolve_definition(struct symbol *existing, struct symbol *incoming)
{
    assert(existing != NULL);
    assert(incoming != NULL);
    assert(strcmp(existing->name, incoming->name) == 0);
    assert(existing->binding != SYMBOL_LOCAL);
    assert(incoming->binding != SYMBOL_LOCAL);

    // Both symbols are references (undefined)
    if (!symbol_is_defined(existing) && !symbol_is_defined(incoming)) {
        uint64_t align = existing->align;
        if (incoming->align > align) {
            align = incoming->align;
        }
        incoming->align = align;
        existing->align = align;
        log_debug("Symbol '%s' is undefined", existing->name);
        return 0;
    }

    // Existing is a definition, incoming is a reference
    if (symbol_is_defined(existing) && !symbol_is_defined(incoming)) {
        override_symbol(incoming, existing);
        return 0;
    }

    // Existing is a reference, incoming is a definition
    if (!symbol_is_defined(existing) && symbol_is_defined(incoming)) {
        override_symbol(existing, incoming);
        return 0;
    }

    // Incoming definition is weak, keep the existing
    if (incoming->binding == SYMBOL_WEAK) {
        override_symbol(incoming, existing);
        return 0;
    }

    // Existing definition is weak, keep the incoming
    if (existing->binding == SYMBOL_WEAK) {
        override_symbol(existing, incoming);
        return 0;
    }

    return ENOTUNIQ;
}
