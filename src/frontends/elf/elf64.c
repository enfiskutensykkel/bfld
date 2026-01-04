#include <objfile_frontend.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <elf.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <logging.h>


//static const uint32_t x86_64_reloc_map[256] = {
//    [R_X86_64_NONE] = RELOC_X86_64_NONE,
//    [R_X86_64_64] = RELOC_X86_64_ABS64,
//    [R_X86_64_PC32] = RELOC_X86_64_PC32,
//    [R_X86_64_32] = RELOC_X86_64_ABS32,
//    [R_X86_64_32S] = RELOC_X86_64_ABS32S,
//    [R_X86_64_PLT32] = RELOC_X86_64_PC32,  // FIXME: Hack because we're static-only for the moment
//};


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


static int parse_elf_file(const uint8_t *file_data, 
                          size_t file_size,
                          struct objfile *objfile, 
                          struct secttab *sections, 
                          struct symtab *symbols)
{
    const Elf64_Ehdr* eh = (const void*) file_data;
    const uint32_t *reloc_map = NULL;
    const char* strtab = NULL;
    uint64_t nsyms = 0;
    const Elf64_Sym *symtab = NULL;
    uint64_t nreltabs = 0;

    // Only allow machine code architectures we support
    switch (eh->e_machine) {
        case EM_X86_64:
            //reloc_map = x86_64_reloc_map;
            break;
        default:
            log_fatal("Unexpected machine architecture");
            return ENOTSUP;
    }

    for (uint64_t shndx = 0; shndx < eh->e_shnum; ++shndx) {
        const Elf64_Shdr *sh = elf_section(eh, shndx);

        log_ctx_push(LOG_CTX_SECTION(lookup_strtab_str(eh, sh->sh_name)));

        switch (sh->sh_type) {
            case SHT_SYMTAB:
                if (symtab == NULL) {
                    log_trace("Symbol table section %lu", shndx);

                    nsyms = sh->sh_size / sh->sh_entsize;
                    symtab = (const Elf64_Sym*) (((const uint8_t*) eh) + sh->sh_offset);
                    strtab = (const char*) ((const uint8_t*) eh) + elf_section(eh, sh->sh_link)->sh_offset;
                } else {
                    log_warning("Unexpected additional symbol tables in file. Symbol table is ignored");
                }
                break;

            case SHT_STRTAB:
                if (eh->e_shstrndx != shndx) {
                    log_trace("String table section %lu", shndx);
                }
                break;

            case SHT_REL:
                log_fatal("Relocation type is unsupported");
                return ENOTSUP;

            case SHT_RELA:
                log_trace("Relocation table section %lu", shndx);
                ++nreltabs;
                break;

            case SHT_PROGBITS:
            case SHT_NOBITS:
            case SHT_INIT_ARRAY:
            case SHT_FINI_ARRAY:
            case SHT_PREINIT_ARRAY:
                if (!(sh->sh_flags & SHF_ALLOC)) {
                    log_debug("Section %lu type %u has data, but SHF_ALLOC is not set", shndx, sh->sh_type);
                }
                break;

            default:
                break;
        }

//        if (!!(sh->sh_flags & SHF_ALLOC)) {
//            switch (sh->sh_type) {
//                case SHT_PROGBITS:
//                    break;
//
//                case SHT_NOBITS:
//                    break;
//
//                case SHT_INIT_ARRAY:
//                case SHT_FINI_ARRAY:
//                case SHT_PREINIT_ARRAY:
//                    log_warning("Section %lu type %u is not implemented yet",
//                            shndx, sh->sh_type);
//                    continue;
//
//                case SHT_NOTE:
//                    log_debug("Skipping note section %lu", shndx);
//                    continue;
//                
//                default:
//                    log_notice("Skipping section %lu type %u, with SHF_ALLOC flag set",
//                            shndx, sh->sh_type);
//                    continue;
//            }
//        }

        log_ctx_pop();
    }

    if (symtab == NULL) {
        log_error("Could not locate symbol table");
        return EINVAL;
    }

    // Parse sections
    //parse_elf_sects(eh, objfile, sections);

    // Parse symbols

    // Parse relocations

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
