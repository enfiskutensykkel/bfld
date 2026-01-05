#include <frontends/objfile.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <elf.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <logging.h>
#include <utils/list.h>
#include <utils/align.h>


//static const uint32_t x86_64_reloc_map[256] = {
//    [R_X86_64_NONE] = RELOC_X86_64_NONE,
//    [R_X86_64_64] = RELOC_X86_64_ABS64,
//    [R_X86_64_PC32] = RELOC_X86_64_PC32,
//    [R_X86_64_32] = RELOC_X86_64_ABS32,
//    [R_X86_64_32S] = RELOC_X86_64_ABS32S,
//    [R_X86_64_PLT32] = RELOC_X86_64_PC32,  // FIXME: Hack because we're static-only for the moment
//};


/*
 * Data structure for tracking ELF sections we want 
 * to revisit after the initial parsing.
 */
struct elf_section_entry
{
    struct list_head entry;
    const Elf64_Shdr* shdr;
};


static bool check_elf_header(const uint8_t *file_data, size_t file_size)
{
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr*) file_data;

    if (file_size <= sizeof(Elf64_Ehdr)) {
        return false;
    }

    if (ehdr->e_ident[EI_MAG0] != ELFMAG0
            || ehdr->e_ident[EI_MAG1] != ELFMAG1
            || ehdr->e_ident[EI_MAG2] != ELFMAG2
            || ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        return false;
    }

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        return false;
    }

    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
        return false;
    }

    if (ehdr->e_shentsize != sizeof(Elf64_Shdr)) {
        return false;
    }

    // Only allow .o files
    if (ehdr->e_type != ET_REL) {
        return false;
    }

    return true;
}


/*
 * Helper function to get a section header with the given index.
 */
static inline const Elf64_Shdr * elf_section(const Elf64_Ehdr *eh, uint64_t idx)
{
    return ((const Elf64_Shdr*) (((const uint8_t*) eh) + eh->e_shoff)) + idx;
}


/*
 * Helper function to look up a string (for example section names) from
 * the global string table.
 */
static const char * lookup_strtab_str(const Elf64_Ehdr *ehdr, uint32_t offset)
{
    if (ehdr->e_shstrndx == SHN_UNDEF) {
        return NULL;
    }

    const Elf64_Shdr *sh = elf_section(ehdr, ehdr->e_shstrndx);
    if (sh->sh_type != SHT_STRTAB) {
        log_warning("ELF section %u has incorrect type", ehdr->e_shstrndx);
    }

    const char *strtab = ((const char*) ehdr) + elf_section(ehdr, ehdr->e_shstrndx)->sh_offset;
    return strtab + offset;
}


/*
 * Helper function to add a section header to a list of sections.
 */
static inline int add_section(struct list_head *list, const Elf64_Shdr *sh)
{
    struct elf_section_entry *entry = malloc(sizeof(struct elf_section_entry));
    if (entry == NULL) {
        return ENOMEM;
    }
    entry->shdr = sh;
    list_insert_tail(list, &entry->entry);
    return 0;
}


/*
 * Are there any symbols that refer to the common section?
 */
static bool scan_common_sym(const Elf64_Ehdr *eh, const Elf64_Shdr *sh)
{
    uint64_t shndx = sh - ((const Elf64_Shdr*) (((const uint8_t*) eh) + eh->e_shoff));

    log_ctx_push(LOG_CTX_SECTION(lookup_strtab_str(eh, sh->sh_name), .lineno = shndx));

    log_trace("Scanning symbol table for common symbols");
    for (uint32_t idx = 1; idx < sh->sh_size / sh->sh_entsize; ++idx) {
        const Elf64_Sym *sym = ((const Elf64_Sym*) (((const uint8_t*) eh) + sh->sh_offset)) + idx;

        if (sym->st_shndx == SHN_COMMON) {
            log_ctx_pop();
            return true;
        }
    }
    
    log_ctx_pop();
    return false;
}


/*
 * Parse ELF file and create sections
 */
static int parse_sections(const Elf64_Ehdr *eh, 
                          struct objfile *objfile, 
                          struct sections *sections,
                          struct list_head *reltabs,
                          struct list_head *symtabs)
{
    sections_reserve(sections, eh->e_shnum);

    log_trace("Scanning sections");

    for (uint64_t shndx = 0; shndx < eh->e_shnum; ++shndx) {
        const Elf64_Shdr *sh = elf_section(eh, shndx);
        const char *shname = lookup_strtab_str(eh, sh->sh_name);

        log_ctx_push(LOG_CTX_SECTION(shname, .lineno = shndx));

        switch (sh->sh_type) {
            case SHT_SYMTAB:
                log_trace("Identified symbol table section");

                if (!list_empty(symtabs)) {
                    log_warning("Multiple symbol tables detected in file");
                }
                
                if (add_section(symtabs, sh) != 0) {
                    log_ctx_pop();
                    return ENOMEM;
                }
                break;

            case SHT_STRTAB:
                if (eh->e_shstrndx != shndx) {
                    log_trace("Identified string table section");
                }
                break;

            case SHT_REL:
                if (sh->sh_type == SHT_REL) {
                    log_error("Relocation type REL is unsupported and will be ignored");
                }
                break;

            case SHT_RELA:
                log_trace("Identified relocation table section");

                if (add_section(reltabs, sh) != 0) {
                    log_ctx_pop();
                    return ENOMEM;
                }
                break;

            case SHT_PROGBITS:
            case SHT_NOBITS:
            case SHT_INIT_ARRAY:
            case SHT_FINI_ARRAY:
            case SHT_PREINIT_ARRAY:
                if (!(sh->sh_flags & SHF_ALLOC)) {
                    log_trace("Section contains data (sh_type %x), but sh_flag SHF_ALLOC is not set", sh->sh_type);
                }
                break;

            case SHT_NULL:
                break;

            case SHT_NOTE:
                break;

            default:
                log_info("Unknown section with sh_type %x", sh->sh_type);
                break;
        }

        // Only extract sections that have SHF_ALLOC set
        if (!(sh->sh_flags & SHF_ALLOC)) {
            log_ctx_pop();
            continue;
        }

        // Only extract sections that are relevant for the code
        if (sh->sh_type != SHT_PROGBITS && sh->sh_type != SHT_NOBITS) {
            switch (sh->sh_type) {
                case SHT_INIT_ARRAY:
                case SHT_FINI_ARRAY:
                case SHT_PREINIT_ARRAY:
                    log_warning("Support for type %u sections is not implemented yet",
                            sh->sh_type);
                    break;

                case SHT_NOTE:
                    log_trace("Skipping note section");
                    break;
                
                default:
                    log_notice("Skipping section with type %u",
                            sh->sh_type);
                    break;
            }
            log_ctx_pop();
            continue;
        }

        enum section_type type = SECTION_ZERO;
        if (sh->sh_type == SHT_PROGBITS) {
            if (!!(sh->sh_flags & SHF_EXECINSTR)) {
                type = SECTION_TEXT;
            } else if (!!(sh->sh_flags & SHF_WRITE)) {
                type = SECTION_DATA;
            } else {
                type = SECTION_RODATA;
            }
        }

        struct section *section = section_alloc(objfile, shndx, shname,
                                                sh->sh_offset, type,
                                                ((const uint8_t*) eh) + sh->sh_offset,
                                                sh->sh_size);
        if (section == NULL) {
            log_ctx_pop();
            return ENOMEM;
        }

        int status = sections_insert(sections, section->idx, section, NULL);
        section_put(section);
        if (status != 0) {
            log_fatal("Could not add section to section table");
            log_ctx_pop();
            return ENOMEM;
        }

        log_trace("Added to section table");
        log_ctx_pop();
    }

    // Insert a fake .common section if any symbols refer to the common section
    list_for_each_entry(s, symtabs, struct elf_section_entry, entry) {
        const Elf64_Shdr *sh = s->shdr;
        uint64_t shndx = sh - ((const Elf64_Shdr*) (((const uint8_t*) eh) + eh->e_shoff));

        log_ctx_push(LOG_CTX_SECTION(lookup_strtab_str(eh, sh->sh_name), .lineno = shndx));

        if (scan_common_sym(eh, sh)) {
            log_debug("Creating .common section");
            struct section *common = section_alloc(objfile, sections->maxidx + 1,
                                                   ".common", 0, SECTION_ZERO, NULL, 0);
            if (common == NULL) {
                log_ctx_pop();
                return ENOMEM;
            }

            int status = sections_insert(sections, common->idx, common, NULL);
            section_put(common);
            if (status != 0) {
                log_ctx_pop();
                return ENOMEM;
            }

            log_ctx_pop();
            break;
        }

        log_ctx_pop();
    }

    return 0;
}


struct symbol * create_symbol(const Elf64_Sym *sym, const char *name)
{
    return NULL;
}


/*
 * Parse the symbol table and extract symbols.
 */
static int parse_symtab(const Elf64_Ehdr *eh, const Elf64_Shdr *sh, const struct sections *sections, 
                        struct symbols *symbols, struct globals *globals)
{
    int status = 0;
    const char* strtab = (const char*) ((const uint8_t*) eh) + elf_section(eh, sh->sh_link)->sh_offset;
    uint64_t shndx = sh - ((const Elf64_Shdr*) (((const uint8_t*) eh) + eh->e_shoff));

    log_ctx_push(LOG_CTX_SECTION(lookup_strtab_str(eh, sh->sh_name), .lineno = shndx));

    if (!symbols_reserve(symbols, sh->sh_size / sh->sh_entsize)) {
        log_ctx_pop();
        return ENOMEM;
    }

    log_trace("Parsing symbol table");
    for (uint32_t idx = 1; idx < sh->sh_size / sh->sh_entsize; ++idx) {
        const Elf64_Sym *sym = ((const Elf64_Sym*) (((const uint8_t*) eh) + sh->sh_offset)) + idx;
        const char *name = strtab + sym->st_name;
        struct section *section = NULL; 
        uint64_t align = 0;
        uint64_t offset = 0;
        uint64_t size = sym->st_size;

        enum symbol_type type = SYMBOL_NOTYPE;
        enum symbol_binding binding = SYMBOL_LOCAL;

        switch (sym->st_shndx) {
            case SHN_UNDEF:
                break;

            case SHN_ABS:
                offset = sym->st_value;
                break;

            case SHN_COMMON:
                section = sections_at(sections, sections->maxidx);
                assert(section != NULL);
                align = sym->st_value;
                if (align > section->align) {
                    section->align = align;
                }
                section->size = align_to(section->size, section->align);
                // FIXME: the size calculation is not correct

                offset = 0;
                break;
            
            default:
                section = sections_at(sections, sym->st_shndx);
                if (section == NULL) {
                    log_error("Symbol '%s' (index %u) refers to invalid segment %u",
                            name, idx, sym->st_shndx);
                    status = EINVAL;
                    goto out;
                }
                offset = sym->st_value;
                break;
        }

        switch (ELF64_ST_BIND(sym->st_info)) {
            case STB_GLOBAL:
                binding = SYMBOL_GLOBAL;
                break;

            case STB_WEAK:
                binding = SYMBOL_WEAK;
                break;

            case STB_LOCAL:
            default:
                binding = SYMBOL_LOCAL;
                break;
        }

        switch (ELF64_ST_TYPE(sym->st_info)) {
            case STT_NOTYPE:
                type = SYMBOL_NOTYPE;
                break;

            case STT_OBJECT:
                type = SYMBOL_OBJECT;
                break;

            case STT_TLS:
                type = SYMBOL_TLS;
                break;

            case STT_SECTION:
                name = section->name;
                type = SYMBOL_SECTION;
                break;

            case STT_FUNC:
                type = SYMBOL_FUNCTION;
                break;

            case STT_COMMON:
                // treat as weak, uninitialized data
                type = SYMBOL_NOTYPE;
                binding = SYMBOL_WEAK;
                break;

            case STT_LOPROC:
            case STT_HIPROC:
                log_warning("Unsupported processor specific symbol type %u", 
                        ELF64_ST_TYPE(sym->st_info));
                continue;

            case STT_FILE:
                log_trace("Ignoring symbol '%s'", name);
                continue;

            default:
                log_warning("Detected symbol '%s' with unknown type %u", 
                        name, ELF64_ST_TYPE(sym->st_info));
                break;
        }

        struct symbol *symbol = symbol_alloc(name, type, binding);
        if (symbol == NULL) {
            status = ENOMEM;
            goto out;
        }
        symbol->align = align;

        if (ELF64_ST_TYPE(sym->st_info) == STT_COMMON) {
            symbol->is_common = true;
        }

        if (offset > 0) {
            status = symbol_bind_definition(symbol, section, offset, size);
            if (status != 0) {
                symbol_put(symbol);
                goto out;
            }
        } 

        struct symbol *existing = symbol;

        if (symbol->binding == SYMBOL_WEAK || symbol->binding == SYMBOL_GLOBAL) {

            status = globals_insert_symbol(globals, symbol, &existing);
            if (status == EEXIST) {
                status = symbol_resolve_definition(existing, symbol);
                if (status != 0) {
                    symbol_put(symbol);
                    goto out;
                }
            } else if (status != 0) {
                symbol_put(symbol);
                goto out;
            }
        }

        // We insert the existing symbol so we are updated on changes to it
        status = symbols_insert(symbols, idx, existing, NULL);
        if (status != 0) {
            symbol_put(symbol);
            goto out;
        }

        symbol_put(symbol);
    }

out:
    log_ctx_pop();
    return status;
}
        
static int parse_elf_file(const uint8_t *file_data, 
                          size_t file_size,
                          struct objfile *objfile, 
                          struct sections *sections, 
                          struct symbols *symbols,
                          struct globals *globals)
{
    int status = 0;
    const Elf64_Ehdr* eh = (const void*) file_data;
    struct list_head reltabs = LIST_HEAD_INIT(reltabs);
    struct list_head symtabs = LIST_HEAD_INIT(symtabs);

    (void) file_size; // unused parameter

    // Only allow machine code architectures we support
    switch (eh->e_machine) {
        case EM_X86_64:
            //const uint32_t *reloc_map = NULL;
            //reloc_map = x86_64_reloc_map;
            break;

        default:
            log_fatal("Unexpected machine architecture (type %x). Only x86-64 (type %x) is supported", 
                    eh->e_machine, EM_X86_64);
            return ENOTSUP;
    }

    // Parse file and create sections
    status = parse_sections(eh, objfile, sections, &reltabs, &symtabs);
    if (status != 0) {
        goto cleanup;
    }

    if (list_empty(&symtabs)) {
        log_error("Could not locate symbol table");
        status = EINVAL;
        goto cleanup;
    }

    // Parse symbol table
    list_for_each_entry_safe(s, &symtabs, struct elf_section_entry, entry) {
        status = parse_symtab(eh, s->shdr, sections, symbols, globals);
        if (status != 0) {
            goto cleanup;
        }
        list_remove(&s->entry);
        free(s);
    }

cleanup:
    list_for_each_entry_safe(s, &symtabs, struct elf_section_entry, entry) {
        list_remove(&s->entry);
        free(s);
    }

    list_for_each_entry_safe(s, &reltabs, struct elf_section_entry, entry) {
        list_remove(&s->entry);
        free(s);
    }

    return status;
}


const struct objfile_frontend elf64_fe = {
    .name = "Elf64",
    .probe_file = check_elf_header,
    .parse_file = parse_elf_file,
};


__attribute__((constructor))
static void elf_fe_init(void)
{
    objfile_frontend_register(&elf64_fe);
}
