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


//static const uint32_t x86_64_reloc_map[256] = {
//    [R_X86_64_NONE] = RELOC_X86_64_NONE,
//    [R_X86_64_64] = RELOC_X86_64_ABS64,
//    [R_X86_64_PC32] = RELOC_X86_64_PC32,
//    [R_X86_64_32] = RELOC_X86_64_ABS32,
//    [R_X86_64_32S] = RELOC_X86_64_ABS32S,
//    [R_X86_64_PLT32] = RELOC_X86_64_PC32,  // FIXME: Hack because we're static-only for the moment
//};


struct reloc_section
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
 * Helper function to add a section with a relocation table to a list.
 */
static inline int add_reloc_sect(struct list_head *list, const Elf64_Shdr *sh)
{
    struct reloc_section *p = malloc(sizeof(struct reloc_section));
    if (p == NULL) {
        return ENOMEM;
    }
    p->shdr = sh;
    list_insert_tail(list, &p->entry);
    return 0;
}


/*
 * Parse the symbol table and extract symbols.
 */
static int parse_symtab(const Elf64_Ehdr *eh, const Elf64_Shdr *sh, 
                        struct sections *sections, struct symbols *symbols, struct globals *globals)
{
    const char* strtab = (const char*) ((const uint8_t*) eh) + elf_section(eh, sh->sh_link)->sh_offset;

    log_ctx_push(LOG_CTX_SECTION(lookup_strtab_str(eh, sh->sh_name)));

    log_trace("Parsing symbol table");
    for (uint32_t idx = 1; idx < sh->sh_size / sh->sh_entsize; ++idx) {
        const Elf64_Sym *sym = ((const Elf64_Sym*) (((const uint8_t*) eh) + sh->sh_offset)) + idx;
        uint8_t type = ELF64_ST_TYPE(sym->st_info);
        uint8_t bind = ELF64_ST_BIND(sym->st_info);
        const char *name = strtab + sym->st_name;

        if (type == STT_SECTION) {
            const struct section *section = sections_at(sections, sym->st_shndx);
            if (section == NULL) {
                log_error("Symbol of type STT_SECTION refers to invalid section %u", sym->st_shndx);
                return EINVAL;
            }
            name = section->name;
        }

        switch (type) {
            case STT_LOPROC:
            case STT_HIPROC:
                log_warning("Unsupported processor specific symbol type %u encounted", type);
                continue;

            case STT_FILE:
                log_trace("Ignoring symbol '%s'", name);
                continue;

            default:
                log_trace("Detected symbol '%s'", name);
                break;
        }
    }

    log_ctx_pop();
    return 0;
}
        
static int parse_elf_file(const uint8_t *file_data, 
                          size_t file_size,
                          struct objfile *objfile, 
                          struct sections *sections, 
                          struct symbols *symbols,
                          struct globals *globals)
{
    const Elf64_Ehdr* eh = (const void*) file_data;
    //const uint32_t *reloc_map = NULL;
    struct list_head relocsects = LIST_HEAD_INIT(relocsects);
    const Elf64_Shdr *symtabsect = NULL;

    // Only allow machine code architectures we support
    switch (eh->e_machine) {
        case EM_X86_64:
            //reloc_map = x86_64_reloc_map;
            break;
        default:
            log_fatal("Unexpected machine architecture");
            return ENOTSUP;
    }

    sections_reserve(sections, eh->e_shnum);

    for (uint64_t shndx = 0; shndx < eh->e_shnum; ++shndx) {
        const Elf64_Shdr *sh = elf_section(eh, shndx);
        const char *shname = lookup_strtab_str(eh, sh->sh_name);

        log_ctx_push(LOG_CTX_SECTION(shname, .lineno = shndx));

        switch (sh->sh_type) {
            case SHT_SYMTAB:
                if (symtabsect == NULL) {
                    log_trace("Identified symbol table section");
                    symtabsect = sh;
                } else {
                    log_warning("Unexpected additional symbol tables in file. Symbol table is ignored");
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

                if (add_reloc_sect(&relocsects, sh) != 0) {
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
                    log_debug("Section contains data (type %u), but SHF_ALLOC is not set", sh->sh_type);
                }
                break;

            default:
                break;
        }

        if (!!(sh->sh_flags & SHF_ALLOC)) {
            struct section *section = NULL;

            switch (sh->sh_type) {
                case SHT_PROGBITS:
                    section = section_alloc(objfile, 
                                            shname, 
                                            SECTION_RODATA,
                                            ((const uint8_t*) eh) + sh->sh_offset,
                                            sh->sh_size);
                    if (section == NULL) {
                        log_ctx_pop();
                        return ENOMEM;
                    }

                    if (sh->sh_flags & SHF_EXECINSTR) {
                        section->type = SECTION_TEXT;
                    } else if (!!(sh->sh_flags & SHF_WRITE)) {
                        section->type = SECTION_DATA;
                    } 
                    section->idx = shndx;
                    section->offset = sh->sh_offset;
                    break;

                case SHT_NOBITS:
                    section = section_alloc(objfile,
                                            shname,
                                            SECTION_ZERO,
                                            NULL, 0);
                    if (section == NULL) {
                        log_ctx_pop();
                        return ENOMEM;
                    }
                    break;

                case SHT_INIT_ARRAY:
                case SHT_FINI_ARRAY:
                case SHT_PREINIT_ARRAY:
                    log_warning("Support for type %u sections is not implemented yet",
                            sh->sh_type);
                    break;

                case SHT_NOTE:
                    log_debug("Skipping note section");
                    break;
                
                default:
                    log_notice("Skipping section with type %u",
                            sh->sh_type);
                    break;
            }

            if (section != NULL) {
                if (sections_insert(sections, shndx, section, NULL) != 0) {
                    section_put(section);
                    log_fatal("Could not add section to section table");
                    log_ctx_pop();
                    return ENOMEM;
                }
                log_trace("Added to section table");
                
                // Section is inserted into the table, 
                // we don't need the reference anymore
                section_put(section);
            }
        }

        log_ctx_pop();
    }

    if (symtabsect == NULL) {
        log_error("Could not locate symbol table");
        return EINVAL;
    }

    parse_symtab(eh, symtabsect, sections, symbols, globals);

    while (!list_empty(&relocsects)) {
        struct reloc_section *p = list_first_entry(&relocsects, struct reloc_section, entry);
        list_remove(&p->entry);
        free(p);
    }

    return 0;
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
