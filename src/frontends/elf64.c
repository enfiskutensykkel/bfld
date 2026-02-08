#include <objectfile_reader.h>
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


/*
 * Data structure for tracking ELF sections we want 
 * to revisit after the initial parsing.
 */
struct elf_section
{
    struct list_head entry;
    const Elf64_Shdr* sh;
};


static bool check_elf_header(const uint8_t *file_data, size_t file_size, uint32_t *march)
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

    *march = ehdr->e_machine;
    return true;
}


/*
 * Helper function to get a section header with the given index.
 */
static inline 
const Elf64_Shdr * elf_section(const Elf64_Ehdr *eh, uint64_t idx)
{
    return ((const Elf64_Shdr*) (((const uint8_t*) eh) + eh->e_shoff)) + idx;
}


/*
 * Helper function to get the symbol at the given index.
 */
static inline 
const Elf64_Sym * elf_symbol(const Elf64_Ehdr *eh, const Elf64_Shdr *sh, uint64_t idx)
{
    const Elf64_Sym *symtab = (const Elf64_Sym*) (((const uint8_t*) eh) + sh->sh_offset);
    return &symtab[idx];
}


/*
 * Helper function to get the name of the symbol at the given index.
 */
static inline
const char * elf_symbol_name(const Elf64_Ehdr *eh, const Elf64_Shdr *sh, uint64_t idx)
{
    const Elf64_Shdr *link = elf_section(eh, sh->sh_link);
    const Elf64_Sym *symtab = (const Elf64_Sym*) (((const uint8_t*) eh) + sh->sh_offset);
    const char *strtab = (const char*) ((const uint8_t*) eh) + link->sh_offset;
    return &strtab[symtab[idx].st_name];
}


/*
 * Helper function to look up a section name.
 */
static const char * elf_section_name(const Elf64_Ehdr *ehdr, const Elf64_Shdr *shdr)
{
    if (ehdr->e_shstrndx == SHN_UNDEF) {
        return NULL;
    }

    // Check if we're using an extended string table
    uint32_t shstrndx = ehdr->e_shstrndx;
    if (shstrndx == SHN_XINDEX) {
        const Elf64_Shdr *nullsh = elf_section(ehdr, 0);
        shstrndx = nullsh->sh_link;
    }

    const Elf64_Shdr *sh = elf_section(ehdr, shstrndx);
    if (sh->sh_type != SHT_STRTAB) {
        log_warning("ELF section %u has incorrect type", ehdr->e_shstrndx);
    }

    const char *strtab = ((const char*) ehdr) + elf_section(ehdr, ehdr->e_shstrndx)->sh_offset;
    return strtab + shdr->sh_name;
}


/*
 * Helper function to add a section header to a list of sections we need to look at.
 */
static inline int add_section(struct list_head *list, const Elf64_Shdr *sh)
{
    struct elf_section *entry = malloc(sizeof(struct elf_section));
    if (entry == NULL) {
        return ENOMEM;
    }
    entry->sh = sh;
    list_insert_tail(list, &entry->entry);
    return 0;
}


/*
 * Parse ELF file and create sections
 */
static int parse_sections(const Elf64_Ehdr *eh, 
                          struct objectfile *objfile, 
                          struct section_table *sections,
                          struct list_head *groups,
                          struct list_head *reltabs,
                          struct list_head *symtabs)
{
    uint64_t shnum = eh->e_shnum;

    if (shnum == 0) {
        const Elf64_Shdr *nullsect = elf_section(eh, 0);
        shnum = nullsect->sh_size;
    }

    section_table_reserve(sections, shnum);

    log_trace("Scanning sections");

    for (uint64_t shndx = 0; shndx < shnum; ++shndx) {
        const Elf64_Shdr *sh = elf_section(eh, shndx);
        const char *shname = elf_section_name(eh, sh);

        log_ctx_push(LOG_CTX_SECTION(shname));

        enum section_type type = SECTION_MAX_TYPES;

        switch (sh->sh_type) {
            case SHT_GROUP:
                if (add_section(groups, sh) != 0) {
                    log_ctx_pop();
                    return ENOMEM;
                }
                break;

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
                    log_warning("Relocation type REL is unsupported and will be ignored");
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
                if (strcmp(shname, ".eh_frame") == 0) {
                    // FIXME: Implement CIE/DWARF parser in the future
                    log_trace("Section is .eh_frame");
                    type = SECTION_UNWIND;
                } else if (!!(sh->sh_flags & SHF_ALLOC)) {
                    if (!!(sh->sh_flags & SHF_WRITE)) {
                        type = SECTION_DATA;
                    } else if (!!(sh->sh_flags & SHF_EXECINSTR)) {
                        type = SECTION_CODE;
                    } else {
                        type = SECTION_READONLY;
                    }
                } else {
                    type = SECTION_METADATA;
                }
                break;

            case SHT_NOBITS:
                if (!!(sh->sh_flags & SHF_ALLOC)) {
                    type = SECTION_ZERO;
                } else {
                    log_debug("Section %s (index %u) is SHT_NOBITS but sh_flag SHF_ALLOC is not set", 
                            shname, shndx, sh->sh_type);
                }
                break;

            case SHT_INIT_ARRAY:
            case SHT_FINI_ARRAY:
            case SHT_PREINIT_ARRAY:
                if (!!(sh->sh_flags & SHF_ALLOC)) {
                    type = SECTION_READONLY;
                } else {
                    log_debug("Section %s (index %u) contains data but sh_flag SHF_ALLOC is not set", 
                            shname, shndx, sh->sh_type);
                }
                
                break;

            case SHT_NULL:
                break;

            case SHT_NOTE:
                type = SECTION_METADATA;
                break;

            default:
                log_warning("Section %s (index %u) has unknown type %x", 
                        shname, shndx, sh->sh_type);
                break;
        }

        // Discard .commend and .annobin.notes
        if (strcmp(shname, ".comment") == 0 || strncmp(shname, ".annobin.", 9) == 0) {
            log_trace("Discarding section %s (index %u)", shname, shndx);
            log_ctx_pop();
            continue;
        }

        // Only extract sections that are relevant for the linker
        if (type >= SECTION_MAX_TYPES) {
            log_ctx_pop();
            continue;
        }

        if (!!(sh->sh_flags & SHF_MERGE)) {
            if (sh->sh_flags & SHF_STRINGS) {
                log_trace("Section is a string merge section");
            } else {
                log_trace("Merge section contains %lu entries of size %u",
                        sh->sh_size / sh->sh_entsize, sh->sh_entsize);
            }

            log_info("Merge sections are not supported yet");
        }

        struct section *section = section_alloc(objfile, shname, type,
                                                ((const uint8_t*) eh) + sh->sh_offset,
                                                sh->sh_size);
        if (section == NULL) {
            log_ctx_pop();
            return ENOMEM;
        }

        bool added = section_table_insert(sections, shndx, section, NULL);
        section_put(section);
        if (!added) {
            log_fatal("Could not add section %llu to section table", shndx);
            log_ctx_pop();
            return ENOMEM;
        }

        log_ctx_pop();
    }

    return 0;
}


/*
 * Parse a relocation table.
 */
static int parse_reltab(const Elf64_Ehdr *eh, 
                        const Elf64_Shdr *sh, 
                        const struct section_table *sects, 
                        const struct symbol_table *syms)
{
    log_ctx_push(LOG_CTX_SECTION(elf_section_name(eh, sh)));

    switch (sh->sh_type) {
        case SHT_REL:
            if (sh->sh_entsize != sizeof(Elf64_Rel)) {
                log_fatal("Expected REL section");
                log_ctx_pop();
                return EINVAL;
            }
            break;

        case SHT_RELA:
            if (sh->sh_entsize != sizeof(Elf64_Rela)) {
                log_fatal("Expected RELA section");
                log_ctx_pop();
                return EINVAL;
            }
            break;

        default:
            log_fatal("Expected relocation table, got invalid section type %u", sh->sh_type);
            log_ctx_pop();
            return EINVAL;
    }

    struct section *sect = section_table_at(sects, sh->sh_info);

    if (sect == NULL) {
        log_fatal("Relocation table refers to unknown section %u", sh->sh_info);
        log_ctx_pop();
        return EINVAL;
    }

    log_trace("Parsing relocation table");
    for (uint64_t idx = 0; idx < sh->sh_size / sh->sh_entsize; ++idx) {

        struct symbol *sym = NULL;
        uint64_t offset = 0;
        uint32_t type = 0;
        int64_t addend = 0;

        if (sh->sh_type == SHT_RELA) {
            const Elf64_Rela *relatab = (const Elf64_Rela*) (((const uint8_t*) eh) + sh->sh_offset);
            const Elf64_Rela *r = &relatab[idx];
        
            type = ELF64_R_TYPE(r->r_info);
            offset = r->r_offset;
            addend = r->r_addend;
            sym = symbol_table_at(syms, ELF64_R_SYM(r->r_info));
        } else {
            const Elf64_Rel *reltab = (const Elf64_Rel*) (((const uint8_t*) eh) + sh->sh_offset);
            const Elf64_Rel *r = &reltab[idx];

            type = ELF64_R_TYPE(r->r_info);
            offset = r->r_offset;
            sym = symbol_table_at(syms, ELF64_R_SYM(r->r_info));
        }

        if (sym == NULL) {
            log_fatal("Relocation entry refers to unknown symbol");
            log_ctx_pop();
            return EINVAL;
        }

        //log_trace("Relocation %lu at offset %zu is relative to symbol '%s'", idx, offset, sym->name);

        struct reloc * reloc = section_add_reloc(sect, offset, sym, type, addend);
        if (reloc == NULL) {
            log_ctx_pop();
            return ENOMEM;
        }
    }

    log_ctx_pop();
    return 0;
}


static int parse_group(const Elf64_Ehdr *eh,
                       const Elf64_Shdr *sh,
                       const struct section_table *sections,
                       struct groups *groups)
{
    // TODO: strategy for same signature, "first one wins", "largest one wins", "make sure all duplicates are same size", "make sure all duplicates have same content"
    // contents: first entry is usually GRP_COMDAT, subsequent entries are indices of sections that belong in list
    // need a seen group set
    // if signature is new, keep all sections listed, add signature to seen set
    // if signature is alreadyy seen, discard (do not extract) every section listed in that group
    // discard the section itself
    
    log_ctx_push(LOG_CTX_SECTION(elf_section_name(eh, sh)));

    const char *signature = elf_symbol_name(eh, elf_section(eh, sh->sh_link), sh->sh_info);
    const uint32_t *entries = (const uint32_t*) (((const uint8_t*) eh) + sh->sh_offset);
    struct group *group = groups_create(groups, signature, sh->sh_size / sizeof(uint32_t));
    bool new_group = group_empty(group);
    bool comdat = true;

    // First entry contains GRP_COMDAT in almost all cases
    if (!(entries[0] & GRP_COMDAT)) {
        // if COMDAT flag is not set, we can not rely on name alone to do deduplication
        log_warning("Section group '%s' is not COMDAT", signature);
        comdat = false;
    }

    for (uint32_t idx = 1; idx < sh->sh_size / sizeof(uint32_t); ++idx) {
        struct section *sect = section_table_at(sections, entries[idx]);
        if (sect != NULL) {
            group_add_section(group, sect);
            if (comdat && !new_group) {
                sect->discard = true;
            }
        }
    }

    // deduplication
    log_ctx_pop();
    return 0;
}


/*
 * Parse the symbol table and extract symbols.
 */
static int parse_symtab(const Elf64_Ehdr *eh, 
                        const Elf64_Shdr *sh, 
                        const struct section_table *sections, 
                        struct symbol_table *symbols)
{
    int status = -1;
    assert(sh->sh_type == SHT_SYMTAB);

    log_ctx_push(LOG_CTX_SECTION(elf_section_name(eh, sh)));

    if (!symbol_table_reserve(symbols, sh->sh_size / sh->sh_entsize)) {
        log_ctx_pop();
        return ENOMEM;
    }

    log_trace("Parsing symbol table");
    for (uint32_t idx = 1; idx < sh->sh_size / sh->sh_entsize; ++idx) {
        const Elf64_Sym *sym = elf_symbol(eh, sh, idx);
        const char *name = elf_symbol_name(eh, sh, idx);
        struct section *section = NULL; 
        uint64_t align = 0;
        uint64_t offset = 0;
        uint64_t size = sym->st_size;

        enum symbol_type type = SYMBOL_MAX_TYPES;
        enum symbol_binding binding = SYMBOL_LOCAL;

        switch (sym->st_shndx) {
            case SHN_UNDEF:
                break;

            case SHN_ABS:
                offset = sym->st_value;
                break;

            case SHN_COMMON:
                align = sym->st_value;
                break;
            
            default:
                section = section_table_at(sections, sym->st_shndx);
                if (section == NULL) {
                    log_debug("Symbol '%s' (index %u, type %u, binding %u) refers to unmapped section %u",
                            name, idx, 
                            ELF64_ST_TYPE(sym->st_info), 
                            ELF64_ST_BIND(sym->st_info), 
                            sym->st_shndx);
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
                align = sym->st_value;
                break;

            case STT_LOPROC:
            case STT_HIPROC:
                log_warning("Unsupported processor specific symbol type %u", 
                        ELF64_ST_TYPE(sym->st_info));
                continue;

            case STT_FILE:
                type = SYMBOL_DEBUG;
                continue;

            default:
                log_warning("Detected symbol '%s' with unknown type %u", 
                        name, ELF64_ST_TYPE(sym->st_info));
                type = STT_NOTYPE;
                break;
        }

        if (type >= SYMBOL_MAX_TYPES) {
            log_warning("Ignoring symbol '%s' with unknown type", name);
            continue;
        }

        struct symbol *symbol = symbol_alloc(name, type, binding);
        if (symbol == NULL) {
            status = ENOMEM;
            goto out;
        }

        bool defined = false;
        if (align > 0) {
            defined = symbol_define_common(symbol, size, align);
        } else if (offset > 0 || section != NULL) {
            defined = symbol_define(symbol, section, offset, size);
        } 
        if ((align > 0 || section != NULL || offset > 0) && !defined) {
            symbol_put(symbol);
            status = EEXIST;
            goto out;
        }

        bool added = symbol_table_insert(symbols, idx, symbol, NULL);
        symbol_put(symbol);
        if (!added) {
            log_fatal("Could not add symbol %llu to symbol table", idx);
            status = ENOMEM;
            goto out;
        }
    }

    log_trace("Parsed symbol table");
    status = 0;

out:
    log_ctx_pop();
    return status;
}

static int parse_elf_file(const uint8_t *file_data, 
                          size_t file_size,
                          struct objectfile *objfile, 
                          struct groups *groups,
                          struct section_table *sections, 
                          struct symbol_table *symbols)
{
    int status = 0;
    const Elf64_Ehdr* eh = (const void*) file_data;
    struct list_head reltabs = LIST_HEAD_INIT(reltabs);
    struct list_head symtabs = LIST_HEAD_INIT(symtabs);
    struct list_head groupsects = LIST_HEAD_INIT(groupsects);

    (void) file_size; // unused parameter
    
    // Parse file and create sections
    status = parse_sections(eh, objfile, sections, &groupsects, &reltabs, &symtabs);
    if (status != 0) {
        goto cleanup;
    }

    if (list_empty(&symtabs)) {
        log_error("Could not locate symbol table");
        status = EINVAL;
        goto cleanup;
    }

    // Parse section groups
    list_for_each_entry_safe(s, &groupsects, struct elf_section, entry) {
        status = parse_group(eh, s->sh, sections, groups);
        if (status != 0) {
            goto cleanup;
        }
        list_remove(&s->entry);
        free(s);
    }

    // Parse symbol table
    list_for_each_entry_safe(s, &symtabs, struct elf_section, entry) {
        status = parse_symtab(eh, s->sh, sections, symbols);
        if (status != 0) {
            goto cleanup;
        }
        list_remove(&s->entry);
        free(s);
    }

    // Parse relocation tables
    list_for_each_entry_safe(s, &reltabs, struct elf_section, entry) {
        status = parse_reltab(eh, s->sh, sections, symbols);
        if (status != 0) {
            goto cleanup;
        }
        list_remove(&s->entry);
        free(s);
    }

cleanup:
    list_for_each_entry_safe(s, &groupsects, struct elf_section, entry) {
        list_remove(&s->entry);
        free(s);
    }

    list_for_each_entry_safe(s, &symtabs, struct elf_section, entry) {
        list_remove(&s->entry);
        free(s);
    }

    list_for_each_entry_safe(s, &reltabs, struct elf_section, entry) {
        list_remove(&s->entry);
        free(s);
    }

    return status;
}


const struct objectfile_reader elf64_frontend = {
    .name = "Elf64",
    .probe_file = check_elf_header,
    .parse_file = parse_elf_file,
};


__attribute__((constructor))
static void elf64_frontend_init(void)
{
    objectfile_reader_register(&elf64_frontend);
}
