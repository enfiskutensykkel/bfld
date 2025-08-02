#include "logging.h"
#include "objfile_loader.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <elf.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "utils/list.h"
#include "x86_64.h"


static const uint32_t reloc_map[256] = {
    [R_X86_64_NONE] = RELOC_X86_64_NONE,
    [R_X86_64_64] = RELOC_X86_64_ABS64,
    [R_X86_64_PC32] = RELOC_X86_64_PC32,
    [R_X86_64_32] = RELOC_X86_64_ABS32,
    [R_X86_64_32S] = RELOC_X86_64_ABS32S,
};


/*
 * Hold some information about the ELF file, in order to do fast
 * lookup at a later point.
 */
struct elf_context
{
    const Elf64_Ehdr *eh;       // pointer to the start of the ELF file (the ELF header)
    const char *strtab;         // symbol string table
    uint64_t nsyms;             // number of symbols in the symbol table
    const Elf64_Sym *symtab;    // pointer to the symbol table
    uint64_t nreltabs;
    const Elf64_Shdr **relsects; // sections with relocations
};


static struct elf_context * create_elf_context(const uint8_t *ptr)
{
    struct elf_context *ctx = malloc(sizeof(struct elf_context));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->eh = (const Elf64_Ehdr*) ptr;
    ctx->strtab = NULL;
    ctx->nsyms = 0;
    ctx->symtab = NULL;
    ctx->nreltabs = 0;
    ctx->relsects = NULL;

    return ctx;
}


static void release_elf_context(void *ctx_data)
{
    if (ctx_data != NULL) {
        struct elf_context *ctx = ctx_data;

        free(ctx->relsects);
        free(ctx);
    }
}


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


static int parse_elf_file(void **ctx_data, const uint8_t *data, size_t size, 
                          enum arch_type *detected_arch)
{
    struct elf_context *ctx = create_elf_context(data);
    if (ctx == NULL) {
        return ENOMEM;
    }

    // Only allow architectures we support
    switch (ctx->eh->e_machine) {
        case EM_X86_64:
            *detected_arch = ARCH_x86_64;
            break;

        default:
            // Unsupported architecture
            release_elf_context(ctx);
            return EINVAL;
    }


    log_trace("First pass");

    for (uint64_t shndx = 0; shndx < ctx->eh->e_shnum; ++shndx) {
        const Elf64_Shdr *sh = elf_section(ctx->eh, shndx);

        log_ctx_push(LOG_CTX_FILE(NULL, NULL, .section = lookup_strtab_str(ctx->eh, sh->sh_name)));

        switch (sh->sh_type) {
            case SHT_SYMTAB:
                if (ctx->symtab == NULL) {
                    log_trace("Symbol table section %lu", shndx);

                    ctx->nsyms = sh->sh_size / sh->sh_entsize;
                    ctx->symtab = (const Elf64_Sym*) (((const uint8_t*) ctx->eh) + sh->sh_offset);
                    ctx->strtab = (const char*) ((const uint8_t*) ctx->eh) + elf_section(ctx->eh, sh->sh_link)->sh_offset;
                } else {
                    log_warning("Unexpected additional symbol tables in file. Symbol table is ignored");
                }
                break;

            case SHT_STRTAB:
                if (ctx->eh->e_shstrndx != shndx) {
                    log_trace("String table section %lu", shndx);
                }
                break;

            case SHT_RELA:
            case SHT_REL:
                log_trace("Relocation table section %lu", shndx);
                ++(ctx->nreltabs);
                break;

            case SHT_PROGBITS:
            case SHT_NOBITS:
            case SHT_INIT_ARRAY:
            case SHT_FINI_ARRAY:
            case SHT_PREINIT_ARRAY:
                if (!(sh->sh_flags & SHF_ALLOC)) {
                    log_debug("Section with data without SHF_ALLOC");
                }
                break;

            default:
                break;
        }

        log_ctx_pop();
    }

    if (ctx->symtab == NULL) {
        log_error("Could not locate symbol table");
        release_elf_context(ctx);
        return EINVAL;
    }

    ctx->relsects = calloc(ctx->nreltabs, sizeof(const Elf64_Shdr*));
    if (ctx->relsects == NULL) {
        release_elf_context(ctx);
        return ENOMEM;
    }

    *ctx_data = ctx;
    return 0;
}


static int parse_elf_sects(void *ctx_data, bool (*emit_section)(void *cb_data, const struct objfile_section*), void *cb_data)
{
    struct elf_context *ctx = ctx_data;
    const Elf64_Ehdr *eh = ctx->eh;
    uint64_t reltabndx = 0;

    log_trace("Second pass");

    for (uint64_t shndx = 0; shndx < eh->e_shnum; ++shndx) {
        const Elf64_Shdr *sh = elf_section(eh, shndx);

        switch (sh->sh_type) {
            case SHT_SYMTAB:
            case SHT_STRTAB:
                continue;

            case SHT_REL:
            case SHT_RELA:
                ctx->relsects[reltabndx++] = sh;
                continue;  // do not continue to parse these now

            default:
                break;
        }

        if (!!(sh->sh_flags & SHF_ALLOC)) {
            struct objfile_section sect = {
                .name = lookup_strtab_str(eh, sh->sh_name),
                .section = shndx,
                .offset = sh->sh_offset,
                .type = SECTION_ZERO,
                .align = sh->sh_addralign,
                .size = sh->sh_type != SHT_NOBITS ? sh->sh_size : 0,
                .content = sh->sh_type != SHT_NOBITS ? ((const uint8_t*) eh) + sh->sh_offset : NULL
            };

            log_ctx_push(LOG_CTX_FILE(NULL, NULL, .section = lookup_strtab_str(eh, sh->sh_name)));

            switch (sh->sh_type) {
                case SHT_PROGBITS:
                    if (sh->sh_flags & SHF_EXECINSTR) {
                        sect.type = SECTION_TEXT;
                    } else if (!!(sh->sh_flags & SHF_WRITE)) {
                        sect.type = SECTION_DATA;
                    } else {
                        sect.type = SECTION_RODATA;
                    }
                    break;

                case SHT_NOBITS:
                    sect.type = SECTION_ZERO;
                    break;

                case SHT_INIT_ARRAY:
                case SHT_FINI_ARRAY:
                case SHT_PREINIT_ARRAY:
                    log_warning("Section %lu with type %u is not implemented yet", shndx, sh->sh_type);
                    break;

                case SHT_NOTE:
                    log_debug("Skipping note section %lu with SHF_ALLOC", shndx);
                    break;

                default:
                    log_notice("Skipping section %lu with SHF_ALLOC flag with type %u", shndx, sh->sh_type);
                    break;
            }

            if (!emit_section(cb_data, &sect)) {
                return ECANCELED;
            }

            log_ctx_pop();
        }
    }

    return 0;
}


static int parse_elf_symtab(void *ctx_data, bool (*emit_symbol)(void *cb_data, const struct objfile_symbol*), void *cb_data)
{
    struct elf_context *ctx = (struct elf_context*) ctx_data;
    const Elf64_Ehdr *eh = ctx->eh;

    log_ctx_push(LOG_CTX_FILE(NULL, NULL, 
                .section = lookup_strtab_str(eh, (((const Elf64_Shdr*) &ctx->symtab[0]) - 1)->sh_name)));

    log_trace("Parsing symbol table");

    for (uint32_t idx = 1; idx < ctx->nsyms; ++idx) {
        const Elf64_Sym *sym = &ctx->symtab[idx];
        uint8_t type = ELF64_ST_TYPE(sym->st_info);
        uint8_t bind = ELF64_ST_BIND(sym->st_info);

        struct objfile_symbol symbol = {
            .name = ctx->strtab + sym->st_name,
            .binding = SYMBOL_LOCAL,
            .type = SYMBOL_NOTYPE,
            .common = false,
            .relative = true,
            .align = 0,
            .offset = 0,
            .section = 0,
        };

        // TODO: if binding is global/weak, if visibility=default or protected, global, otherwise local

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
                symbol.type = SYMBOL_OBJECT;
                break;

            case STT_COMMON:
                // treat as weak, uninitialized data
                symbol.type = SYMBOL_NOTYPE;
                symbol.binding = SYMBOL_WEAK;
                break;

            case STT_SECTION:
            default:
                continue;  // do not emit this
        }

        switch (sym->st_shndx) {
            case SHN_UNDEF:
                symbol.section = 0;
                break;

            case SHN_ABS:
                symbol.relative = false;
                symbol.section = 0;
                symbol.offset = sym->st_value;
                break;

            case SHN_COMMON:
                symbol.common = true;
                symbol.section = 0;
                symbol.align = sym->st_value;
                break;

            default:
                symbol.section = sym->st_shndx;
                symbol.offset = sym->st_value;
                break;
        }

        log_trace("Extracting symbol '%s'", symbol.name);
        if (!emit_symbol(cb_data, &symbol)) {
            log_ctx_pop();
            return ECANCELED;
        }
    }

    log_ctx_pop();
    return 0;
}


static int parse_elf_relocs(void *ctx_data, bool (*emit_reloc)(void *cb_data, const struct objfile_relocation*), void *cb_data)
{
    struct elf_context *ctx = ctx_data;

    for (uint64_t idx = 0; idx < ctx->nreltabs; ++idx) {
        const Elf64_Shdr *sh = ctx->relsects[idx];
        const Elf64_Rel *reltab = NULL;
        const Elf64_Rela *relatab = NULL;

        log_ctx_push(LOG_CTX_FILE(NULL, NULL, .section = lookup_strtab_str(ctx->eh, sh->sh_name)));

        log_trace("Parsing relocation table");

        switch (sh->sh_type) {
            case SHT_REL:
                reltab = (const Elf64_Rel*) (((const uint8_t*) ctx->eh) + sh->sh_offset);
                assert(sh->sh_entsize == sizeof(Elf64_Rel));
                break;

            case SHT_RELA:
                relatab = (const Elf64_Rela*) (((const uint8_t*) ctx->eh) + sh->sh_offset);
                assert(sh->sh_entsize == sizeof(Elf64_Rela));
                break;

            default:
                log_error("Invalid section type %u", sh->sh_type);
                log_ctx_pop();
                continue;
        }

        for (uint64_t idx = 0; idx < sh->sh_size / sh->sh_entsize; ++idx) {
            struct objfile_relocation rel = {
                .section = sh->sh_info,
                .offset = 0,
                .sectionref = 0,
                .commonref = false,
                .symbol = NULL,
                .type = 0,
                .addend = 0
            };

            const Elf64_Sym *sym = NULL;

            if (relatab != NULL) {
                const Elf64_Rela *r = &relatab[idx];
                
                rel.offset = r->r_offset;
                rel.type = reloc_map[ELF64_R_TYPE(r->r_info)];
                rel.addend = r->r_addend;
                sym = &ctx->symtab[ELF64_R_SYM(r->r_info)];
                
            } else {
                const Elf64_Rel *r = &reltab[idx];

                rel.offset = r->r_offset;
                rel.type = reloc_map[ELF64_R_TYPE(r->r_info)];
                sym = &ctx->symtab[ELF64_R_SYM(r->r_info)];
            }

            switch (ELF64_ST_TYPE(sym->st_info)) {
                case STT_SECTION:
                    rel.sectionref = sym->st_shndx;
                    break;

                case STT_COMMON:
                    rel.commonref = true;
                    break;

                case STT_NOTYPE:
                case STT_FUNC:
                case STT_OBJECT:
                    rel.symbol = ctx->strtab + sym->st_name;
                    break;

                default:
                    log_error("Unsupported relocation");
                    continue;
            }

            if (rel.type == 0) {
                log_error("Unsupported relocation type");
                continue;
            }

            if (!emit_reloc(cb_data, &rel)) {
                log_ctx_pop();
                return ECANCELED;
            }

            log_ctx_pop();
        }
    }

    return 0;
}


const struct objfile_loader elf_loader = {
    .name = "elf64loader",
    .probe = check_elf_header,
    .scan_file = parse_elf_file,
    .extract_sections = parse_elf_sects,
    .extract_symbols = parse_elf_symtab,
    .extract_relocations = parse_elf_relocs,
    .release = release_elf_context,
};


OBJFILE_LOADER_INIT static void elf_loader_init(void)
{
    objfile_loader_register(&elf_loader);
}
