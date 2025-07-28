#include "logging.h"
#include "objfile_loader.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <elf.h>
#include <errno.h>
#include <string.h>


/*
 * Hold some information about the ELF file, in order to do fast
 * lookup at a later point.
 */
struct elf_file
{
    const Elf64_Ehdr *eh;       // pointer to the start of the ELF file (the ELF header)
    const char *strtab;         // symbol string table
    uint64_t nsyms;               // number of symbols in the symbol table
    const Elf64_Sym *symtab;    // pointer to the symbol table
    const Elf64_Sym *sect_syms[];// section symbol map, used for relocations
};


/*
 * Check if the file has the magic ELF64 signature.
 */
static bool check_elf_header(const uint8_t *ptr, size_t size)
{
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr*) ptr;

    if (size <= sizeof(Elf64_Ehdr)) {
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
static const char * elf_lookup_str(const Elf64_Ehdr *ehdr, uint32_t offset)
{
    if (ehdr->e_shstrndx == SHN_UNDEF) {
        return NULL;
    }

    const char *strtab = ((const char*) ehdr) + elf_section(ehdr, ehdr->e_shstrndx)->sh_offset;
    return strtab + offset;
}


static int parse_elf_file(void **ctx_data, const uint8_t *data, size_t size)
{
    const Elf64_Ehdr *eh = (const Elf64_Ehdr*) data;

    if (!check_elf_header(data, size)) {
        return EBADF;
    }

    uint64_t nsyms = 0;
    const char *strtab = NULL;
    const Elf64_Sym *symtab = NULL;

    // Find the symbol section
    for (uint64_t shndx = 0; shndx <= eh->e_shnum; ++shndx) {
        const Elf64_Shdr *sh = elf_section(eh, shndx);

        if (sh->sh_type == SHT_SYMTAB) {
            nsyms = sh->sh_size / sh->sh_entsize;
            symtab = (const Elf64_Sym*) (((const uint8_t*) eh) + sh->sh_offset);
            strtab = (const char*) ((const uint8_t*) eh) + elf_section(eh, sh->sh_link)->sh_offset;
        }
    }

    if (symtab == NULL) {
        log_fatal("Could not locate symbol table");
    }

    struct elf_file *ctx = malloc(sizeof(struct elf_file) + sizeof(const Elf64_Sym*) * nsyms);
    if (ctx == NULL) {
        return ENOMEM;
    }

    ctx->eh = eh;
    ctx->strtab = strtab;
    ctx->nsyms = nsyms;
    ctx->symtab = symtab;
    memset(&ctx->sect_syms[0], 0, sizeof(const Elf64_Sym*) * ctx->nsyms);

    *ctx_data = ctx;
    return 0;
}


static int parse_elf_symtab(void *ctx, bool (*emit_symbol)(void *user, const struct objfile_symbol*), void *user)
{
    struct elf_file *ef = (struct elf_file*) ctx;

    for (uint32_t idx = 1; idx < ef->nsyms; ++idx) {
        const Elf64_Sym *sym = &ef->symtab[idx];
        uint8_t type = ELF64_ST_TYPE(sym->st_info);
        uint8_t bind = ELF64_ST_BIND(sym->st_info);

        struct objfile_symbol symbol = {
            .name = ef->strtab + sym->st_name,
            .value = sym->st_value,
            .defined = sym->st_shndx != SHN_UNDEF,
            .sect_idx = sym->st_shndx,
            .size = sym->st_size
        };

        switch (bind) {
            case STB_LOCAL:
                symbol.binding = SYMBOL_LOCAL;
                break;

            case STB_GLOBAL:
                symbol.binding = SYMBOL_GLOBAL;
                break;

            case STB_WEAK:
                symbol.binding = SYMBOL_WEAK;
                break;

            default:
                symbol.binding = SYMBOL_LOCAL;
                break;
        }

        switch (type) {
            case STT_NOTYPE:
                symbol.type = SYMBOL_NOTYPE;
                break;

            case STT_FUNC:
                symbol.type = SYMBOL_FUNCTION;
                break;

            case STT_OBJECT:
                symbol.type = SYMBOL_DATA;
                break;

            case STT_SECTION:
                ef->sect_syms[idx] = sym;
                continue;  // do not emit section type

            case STT_COMMON:
                // treat as weak, uninitialized data
                symbol.type = SYMBOL_NOTYPE;
                symbol.binding = SYMBOL_WEAK;
                break;

            default:
                continue;   
        }

        if (!emit_symbol(user, &symbol)) {
            return ECANCELED;
        }
    }

    return 0;
}


const struct objfile_loader elf_loader = {
    .name = "elfloader",
    .probe = check_elf_header,
    .parse_file = parse_elf_file,
    .extract_symbols = parse_elf_symtab,
    .release = free,
};


OBJFILE_LOADER_INIT static void elf_loader_init(void)
{
    objfile_loader_register(&elf_loader);
}
