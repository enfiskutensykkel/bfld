#include "symbol.h"
#include "section.h"
#include "logging.h"
#include "objectfile.h"
#include "utils/hash.h"
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
        if (symbol_is_defined(sym)) {
            symbol_undefine(sym);
        }
        free(sym->name);
        free(sym);
    }
}


struct symbol * symbol_alloc(const char *name, enum symbol_type type, 
                             enum symbol_binding binding)
{
    switch (type) {
        case SYMBOL_NOTYPE:
        case SYMBOL_OBJECT:
        case SYMBOL_TLS:
        case SYMBOL_SECTION:
        case SYMBOL_FUNCTION:
        case SYMBOL_DEBUG:
            break;
        default:
            log_error("Invalid symbol type");
            return NULL;
    }

    struct symbol *sym = malloc(sizeof(struct symbol));
    if (sym == NULL) {
        return NULL;
    }

    sym->name = malloc(strlen(name) + 1);
    if (sym->name == NULL) {
        free(sym);
        return NULL;
    }
    strcpy(sym->name, name);

    sym->hash = hash_fnv1a_32(sym->name, strlen(name));
    if (sym->hash == 0) {
        sym->hash = 1;
    }
    sym->binding = binding;
    sym->type = type;
    sym->refcnt = 1;
    sym->align = 0;
    sym->size = 0;
    sym->is_absolute = false;
    sym->is_common = false;
    //sym->is_used = false;
    sym->section = NULL;
    sym->offset = 0;
    sym->visibility = SYMBOL_PUBLIC;

    return sym;
}


bool symbol_define_common(struct symbol *sym, uint64_t size, uint64_t align)
{
    assert(sym != NULL);

    if (symbol_is_defined(sym)) {
        log_error("Redefinition of symbol '%s' as common symbol", sym->name);
        return false;
    }

    if (sym->is_common) {
        log_error("Redefinition of common symbol '%s', symbol was already defined", sym->name);
        return false;
    }
    
    assert(sym->section == NULL);
    sym->is_absolute = false;
    sym->offset = 0;
    sym->size = size;
    sym->align = align;
    sym->is_common = true;

    return true;
}


bool symbol_define(struct symbol *sym, struct section *section,
                   uint64_t offset, uint64_t size)
{
    assert(sym != NULL);    

    // Check that we are not overwriting a previous strong definition
    if (symbol_is_defined(sym) && sym->binding != SYMBOL_WEAK && !sym->is_common) {
        if (sym->is_absolute) {
            log_error("Redefinition for absolute symbol '%s' at address 0x%llx, was already defined at address 0x%llx", 
                    sym->name, offset, sym->offset);
        } else {
            log_error("Redefinition for symbol '%s'", sym->name);
            const char *filename = sym->section->objfile != NULL ? sym->section->objfile->name : NULL;
            log_ctx_push(LOG_CTX(.file = filename, .section = sym->section->name));
            log_error("Symbol '%s' was previously defined here", sym->name);
            log_ctx_pop();
        }
        return false;
    }

    struct section *old_section = sym->section;
    sym->size = size;
    sym->is_common = false;  // symbol is now defined, it can not be common

    if (section == NULL) {
        // This is an absolute definition
        sym->section = NULL;
        sym->is_absolute = true;
        sym->offset = offset;
        sym->align = 0;  
        log_debug("Absolute symbol '%s' is defined at address 0x%lx", 
                sym->name, sym->offset);

    } else {
        // This is a relative definition
        sym->is_absolute = false;
        sym->offset = offset;
        sym->section = section_get(section);
        section_add_symbol_reference(section, sym);

        //if (sym->is_used && sym->section->discarded) {
        //    // This should not really happen, but if it does notify the user about it
        //    log_warning("Symbol '%s' is marked as in use, but section it is defined in is discarded", sym->name);
        //}
    }

    // Release old section
    if (old_section != NULL) {
        if (old_section != sym->section) {
            section_remove_symbol_reference(old_section, sym);
        }
        section_put(old_section);
    }

    return true;
}


void symbol_undefine(struct symbol *sym)
{
    if (sym->section != NULL) {
        section_remove_symbol_reference(sym->section, sym);
        section_put(sym->section);
        sym->section = NULL;
    }
    sym->is_common = false;
    sym->size = 0;
    sym->align = 0;
    sym->offset = 0;
    sym->is_absolute = false;
//    sym->is_used = false;
}


bool symbol_merge(struct symbol *existing, const struct symbol *incoming)
{
    assert(existing != NULL && "Invalid argument for existing symbol");
    assert(incoming != NULL && "Invalid argument for incoming symbol");
    assert(existing->binding != SYMBOL_LOCAL && "Existing symbol can not be SYMBOL_LOCAL");
    assert(incoming->binding != SYMBOL_LOCAL && "Incoming symbol can not be SYMBOL_LOCAL");
    assert(existing->hash == incoming->hash && "Existing and incoming symbols are not the same symbol");

//    if (incoming->is_used) {
//        log_debug("Marking symbol '%s' as used", existing->name);
//        existing->is_used = true;
//    }

    if (incoming->visibility > existing->visibility) {
        log_debug("Strictening visibility for symbol '%s'", existing->name);
        existing->visibility = incoming->visibility;
    }

    if (existing->is_common && incoming->is_common) {
        if (incoming->align > existing->align) {
            existing->align = incoming->align;
        }

        if (incoming->size > existing->size) {
            existing->size = incoming->size;
        }

        if (existing->binding == SYMBOL_WEAK && incoming->binding == SYMBOL_GLOBAL) {
            existing->binding = SYMBOL_GLOBAL;
        }

        log_debug("Updated alignment and size of common symbol '%s'", existing->name);
        return true;
    }

    // Existing is undefined, incoming is common, upgrade undefined to a common
    if (!symbol_is_defined(existing) && incoming->is_common) {
        existing->is_common = true;
        existing->size = incoming->size;
        existing->align = incoming->align;
        existing->type = incoming->type;
        log_debug("Upgraded undefined symbol '%s' to common symbol", existing->name);
        return true;
    }

    // If incoming is an undefined symbol reference, keep existing definition
    if (!symbol_is_defined(incoming)) {
        return true;
    }

    if (!existing->is_common) {

        // If incoming definition is weak, keep existing definition
        if (symbol_is_defined(existing) && incoming->binding == SYMBOL_WEAK) {
            return true;
        }

        // If existing definition is strong and incoming definition is strong,
        // we have a multiple definition error
        if (symbol_is_defined(existing) && existing->binding != SYMBOL_WEAK) {
            if (existing->is_absolute) {
                log_error("Multiple definitions for absolute symbol '%s', previously defined at address 0x%lx",
                        existing->name, existing->offset);

            } else {
                log_error("Multiple definitions for symbol '%s'", existing->name);
                const char *filename = existing->section->objfile != NULL ? existing->section->objfile->name : NULL;
                log_ctx_push(LOG_CTX(.file = filename, .section = existing->section->name));
                log_error("Symbol '%s' was previously defined here", existing->name);
                log_ctx_pop();
            }
            return false;
        }
    }

    bool success = symbol_define(existing, incoming->section, 
                                 incoming->offset, incoming->size);
    if (!success) {
        return false;
    }

    existing->binding = incoming->binding;
    existing->type = incoming->type;

    return true;
}


//bool symbol_is_alive(const struct symbol *sym)
//{
//    // Symbols that are kept, exported or referenced by a relocation
//    // are always considered "alive"
//    if (sym->is_used) {
//        return true;
//    }
//
//    if (!symbol_is_defined(sym)) {
//        return false; // disregard undefined symbols
//    }
//
//    if (sym->is_absolute || sym->section == NULL) {
//        return false; // disregard absolute symbols that aren't used
//    }
//
//    return !sym->section->discarded;
//}
