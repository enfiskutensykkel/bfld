#include "list.h"
#include "bfld_vm.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <elf.h>


struct ar_symbol
{
    struct list_head list_entry;
    uint32_t offset;  // Offset in archive file to symbol definition
    char name[];      // Name of the symbol
};


static bool read_member_header(FILE *fp, struct ar_header *hdr)
{
    // Try to read an archive member header
    if (fread(hdr, sizeof(struct ar_header), 1, fp) <= 0) {
        return false;
    }

    // Check if the archive member header ends with known signature
    if (strncmp(hdr->end, AR_END, 2) != 0) {
        return false;
    }

    return true;
}


static bool check_elf64_header(const Elf64_Ehdr *ehdr)
{
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

    return true;
}


static const Elf64_Shdr * get_section_header(const Elf64_Ehdr *ehdr, uint16_t idx)
{
    return ((const Elf64_Shdr*) (((const char*) ehdr) + ehdr->e_shoff)) + idx;
}


static const char * lookup_string(const Elf64_Ehdr *ehdr, uint64_t offset)
{
    if (ehdr->e_shstrndx == SHN_UNDEF) {
        return NULL;
    }

    const char *strtab = ((const char*) ehdr) \
                         + get_section_header(ehdr, ehdr->e_shstrndx)->sh_offset;

    return strtab + offset;
}


static bool parse_symtab(const Elf64_Ehdr *ehdr, const Elf64_Shdr *symtab)
{
    if (symtab->sh_type != SHT_SYMTAB) {
        return false;
    }

    size_t nent = symtab->sh_size / symtab->sh_entsize;

    const Elf64_Sym *entries = (const Elf64_Sym*) (((const char*) ehdr) + symtab->sh_offset);
    const char *strtab = ((const char*) ehdr) \
                         + get_section_header(ehdr, symtab->sh_link)->sh_offset;

    for (size_t idx = 1; idx < nent; ++idx) {
        const Elf64_Sym *sym = &entries[idx];

        const char *bind = NULL;
        switch (ELF64_ST_BIND(sym->st_info)) {
            case STB_LOCAL:
                bind = "local";
                break;
            case STB_GLOBAL:
                bind = "global";
                break;
            case STB_WEAK:
                bind = "weak";
                break;
            default:
                bind = "UNKNOWN";
                break;
        }

        const char *type = NULL;
        switch (ELF64_ST_TYPE(sym->st_info)) {
            case STT_OBJECT:
                type = "object";
                break;
            case STT_FUNC:
                type = "function";
                break;
            case STT_SECTION:
                type = "section";
                break;
            case STT_FILE:
                type = "file";
                break;
            case STT_NOTYPE:
                type = "no-type";
                break;
            default:
                type = "OTHER";
                break;
        }

        const char *name = strtab + sym->st_name;
        printf("symbol %s (type=%s bind=%s) has value %lx\n", name, type, bind, sym->st_value);
    }

    return true;
}


static bool parse_elf64(struct bfld_vm *vm, const void *data)
{
    if (data == NULL) {
        return false;
    }

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr*) data;
    if (!check_elf64_header(ehdr)) {
        return false;
    }

    for (uint16_t sh = 0; sh < ehdr->e_shnum; ++sh) {
        const Elf64_Shdr *shdr = get_section_header(ehdr, sh);
        
        if (shdr->sh_type == SHT_SYMTAB) {
            printf("section %s contains symbol table\n", lookup_string(ehdr, shdr->sh_name));
            parse_symtab(ehdr, shdr);
        }

        if (shdr->sh_type == SHT_RELA || shdr->sh_type == SHT_REL) {
            printf("section %s contains relocation table\n", lookup_string(ehdr, shdr->sh_name));
        }

        if (shdr->sh_type == SHT_DYNAMIC) {
            printf("section %s contains dynamic linking info\n", lookup_string(ehdr, shdr->sh_name));
        }
    }

    return true;
}


static size_t get_member_size(const struct ar_header *hdr)
{
    char buffer[11];
    memset(buffer, 0, 11);
    strncpy(buffer, hdr->size, 10);
    return strtoull(buffer, NULL, 10);
}


static void * read_member_data(FILE *fp, const struct ar_header *hdr)
{
    size_t membsz = get_member_size(hdr);

    void *data = malloc(membsz);
    if (data == NULL) {
        return NULL;
    }

    if (fread(data, 1, membsz, fp) != membsz) {
        free(data);
        return NULL;
    }

    return data;
}


static int read_header_references(FILE *fp, const struct ar_header *hdr, char **refs)
{
    size_t refsz = get_member_size(hdr);

    *refs = realloc(*refs, refsz);
    if (fread(*refs, refsz, 1, fp) <= 0) {
        return EINVAL;
    }

    return 0;
}


static int get_member_name(const struct ar_header *hdr, 
                           const char *refs, char **name)
{
    size_t len = 0;
    for (; hdr->name[len] != '/'; ++len);

    if (refs != NULL && len == 0) {
        char buffer[16];
        memset(buffer, 0, 16);
        strncpy(buffer, &hdr->name[1], 15);

        size_t offset = strtoull(buffer, NULL, 10);
        for (; refs[offset + len] != '/'; ++len);
        
        *name = malloc(len + 1);
        strncpy(*name, &refs[offset], len);
        (*name)[len] = '\0';

    } else if (len > 0) {
        *name = malloc(len + 1);
        strncpy(*name, hdr->name, len);
        (*name)[len] = '\0';

    } else {
        *name = NULL;
        return EINVAL;
    }

    return 0;
}


static int read_symbol_lut_32(FILE *fp, const struct ar_header *hdr, struct list_head *symbols)
{
    if (strncmp(hdr->name, "/ ", 2) != 0) {
        return EINVAL;
    }

    uint32_t num_entries = 0;
    if (fread(&num_entries, sizeof(uint32_t), 1, fp) <= 0) {
        return EINVAL;
    }
    num_entries = ntohl(num_entries);

    uint32_t *offsets = calloc(num_entries, sizeof(uint32_t));
    if (offsets == NULL) {
        return ENOMEM;
    }

    if (fread(offsets, sizeof(uint32_t), num_entries, fp) != num_entries) {
        free(offsets);
        return EINVAL;
    }

    size_t total_bytes = 0;
    for (uint32_t i = 0; i < num_entries; ++i) {
        int c;
        size_t len = 0;

        fpos_t pos;
        fgetpos(fp, &pos);

        while ((c = fgetc(fp)) > 0) {
            ++len;
        }

        struct ar_symbol *symbol = malloc(sizeof(struct ar_symbol) + len + 1);
        if (symbol == NULL) {
            free(offsets);
            return ENOMEM;
        }

        symbol->offset = ntohl(offsets[i]);
        fsetpos(fp, &pos);
        total_bytes += fread(symbol->name, 1, len+1, fp);

        list_append_entry(symbols, &symbol->list_entry);
    }

    // Archive data sections are 2-byte aligned
    if (total_bytes % 2 != 0) {
        fgetc(fp); 
    }

    free(offsets);
    return 0;
}


static void free_symbols(struct list_head *symbols) 
{
    list_for_each_node(it, symbols, struct ar_symbol, list_entry) {
        list_remove_entry(&it->list_entry);
        free(it);
    }
}


int bfld_vm_load(FILE *fp, struct bfld_vm **vm)
{
    char magic[AR_MAGIC_SIZE];
    struct ar_header header;
    struct list_head symbols = LIST_HEAD_INIT(symbols);
    char *references = NULL;

    *vm = NULL;

    // Try to read the magic archive file signature
    if (fread(magic, AR_MAGIC_SIZE, 1, fp) <= 0) {
        return EINVAL;
    } 
    
    // Not a recognized archive file
    if (strncmp(magic, AR_MAGIC, AR_MAGIC_SIZE) != 0) {
        return EINVAL;
    }

    *vm = malloc(sizeof(struct bfld_vm));
    if (vm == NULL) {
        return ENOMEM;
    }
    list_head_init(&(*vm)->sections);
    list_head_init(&(*vm)->symbols);

    while (read_member_header(fp, &header)) {
        if (strncmp(header.name, "/ ", 2) == 0) {
            if (!list_empty(&symbols)) {
                free_symbols(&symbols);
                free(references);
                bfld_vm_free(vm);
                return EINVAL;
            }

            if (read_symbol_lut_32(fp, &header, &symbols) != 0) {
                free_symbols(&symbols);
                free(references);
                bfld_vm_free(vm);
                return EINVAL;
            }

        } else if (strncmp(header.name, "/SYM64/", 7) == 0) {
            free_symbols(&symbols);
            free(references);
            bfld_vm_free(vm);
            return ENOTSUP;

        } else if (strncmp(header.name, "//", 2) == 0) {
            if (read_header_references(fp, &header, &references) != 0) {
                free_symbols(&symbols);
                free(references);
                bfld_vm_free(vm);
                return EINVAL;
            }

        } else {
            char *member_name = NULL;

            if (get_member_name(&header, references, &member_name) != 0) {
                free_symbols(&symbols);
                free(member_name);
                free(references);
                bfld_vm_free(vm);
                return EINVAL;
            }

            size_t member_size = get_member_size(&header);

            void *data = read_member_data(fp, &header);
            if (!parse_elf64(*vm, data)) {
                free_symbols(&symbols);
                free(data);
                free(member_name);
                free(references);
                bfld_vm_free(vm);
                return EINVAL;
            }
            
            free(member_name);
            free(data);
        }
    }

    free_symbols(&symbols);
    free(references);
    return 0;
}


void bfld_vm_free(struct bfld_vm **vm)
{
    if (*vm != NULL) {
        struct bfld_vm *v = *vm;
        
        list_for_each_node(it, &v->sections, struct bfld_vm_section, list_entry) {
            list_remove_entry(&it->list_entry);
            free(it);
        }

        list_for_each_node(it, &v->symbols, struct bfld_vm_symbol, list_entry) {
            list_remove_entry(&it->list_entry);
            free(it);
        }

        free(*vm);
        *vm = NULL;
    }
}
