#include "bfld_list.h"
#include "bfld_vm.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <elf.h>


#define AR_MAGIC        "!<arch>\n"
#define AR_MAGIC_SIZE   8
#define AR_END          "`\n"


struct ar_header
{
    char name[16];  // Member file name
    char date[12];  // File modification timestamp
    char uid[6];    // User ID in ASCII decimal
    char gid[6];    // Group ID, in ASCII decimal
    char mode[8];   // File mode, in ASCII octal
    char size[10];  // File size, in ASCII decimal
    char end[2];    // Always contains AR_END
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


static const char * lookup_string(const Elf64_Ehdr *ehdr, uint32_t offset)
{
    if (ehdr->e_shstrndx == SHN_UNDEF) {
        return NULL;
    }

    const char *strtab = ((const char*) ehdr) \
                         + get_section_header(ehdr, ehdr->e_shstrndx)->sh_offset;

    return strtab + offset;
}


static bool parse_elf64(struct bfld_vm *vm, const char *name, const void *data)
{
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr*) data;
    if (!check_elf64_header(ehdr)) {
        return false;
    }

    for (uint16_t sh = 0; sh < ehdr->e_shnum; ++sh) {
        const Elf64_Shdr *shdr = get_section_header(ehdr, sh);

        
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


static int read_symbol_lut_32(FILE *fp, const struct ar_header *hdr, struct bfld_vm *vm)
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

    for (uint32_t i = 0; i < num_entries; ++i) {
        int c;
        size_t len = 0;

        fpos_t pos;
        fgetpos(fp, &pos);

        while ((c = fgetc(fp)) > 0) {
            ++len;
        }

        struct bfld_vm_sym *symbol = malloc(sizeof(struct bfld_vm_sym) + len + 1);
        if (symbol == NULL) {
            free(offsets);
            return ENOMEM;
        }

        symbol->bytecode_idx = ntohl(offsets[i]);
        fsetpos(fp, &pos);
        fread(symbol->name, 1, len+1, fp);
        symbol->len = len;

        list_append(&vm->syms, &symbol->list_entry);
    }

    free(offsets);
    return 0;
}


int bfld_vm_load(FILE *fp, struct bfld_vm **vm)
{
    char magic[AR_MAGIC_SIZE];
    struct ar_header header;
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
    list_head_init(&(*vm)->bytecode);
    list_head_init(&(*vm)->syms);

    while (read_member_header(fp, &header)) {
        if (strncmp(header.name, "/ ", 2) == 0) {
            if (!list_empty(&(*vm)->syms)) {
                free(references);
                bfld_vm_free(vm);
                return EINVAL;
            }

            if (read_symbol_lut_32(fp, &header, *vm) != 0) {
                free(references);
                bfld_vm_free(vm);
                return EINVAL;
            }

        } else if (strncmp(header.name, "/SYM64/", 7) == 0) {
            free(references);
            bfld_vm_free(vm);
            return ENOTSUP;

        } else if (strncmp(header.name, "//", 2) == 0) {
            if (read_header_references(fp, &header, &references) != 0) {
                free(references);
                bfld_vm_free(vm);
                return EINVAL;
            }

        } else {
            char *member_name = NULL;

            if (get_member_name(&header, references, &member_name) != 0) {
                free(member_name);
                free(references);
                bfld_vm_free(vm);
                return EINVAL;
            }

            void *data = read_member_data(fp, &header);
            if (!parse_elf64(*vm, member_name, data)) {
                free(member_name);
                free(references);
                bfld_vm_free(vm);
                return EINVAL;
            }
            
            free(member_name);
            free(data);
        }
    }

    free(references);
    return 0;
}


void bfld_vm_free(struct bfld_vm **vm)
{
    if (*vm != NULL) {
        struct bfld_vm *v = *vm;
        
        list_for_each_node(it, &v->bytecode, struct bfld_vm_bytecode, list_entry) {
            list_remove(&it->list_entry);
            free(it);
        }

        list_for_each_node(it, &v->syms, struct bfld_vm_sym, list_entry) {
            list_remove(&it->list_entry);
            free(it);
        }

        free(*vm);
        *vm = NULL;
    }
}
