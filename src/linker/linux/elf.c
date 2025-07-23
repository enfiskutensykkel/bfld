#include "elf.h"
#include "../objfile.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
#include <stdio.h>


bool elf_check_magic(const void *ptr)
{
    const Elf64_Ehdr *ehdr = ptr;

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


const Elf64_Shdr * elf_section(const Elf64_Ehdr *ehdr, uint16_t idx)
{
    return ((const Elf64_Shdr*) (((const char*) ehdr) + ehdr->e_shoff)) + idx;
}


const char * elf_lookup_str(const Elf64_Ehdr *ehdr, uint32_t offset)
{
    if (ehdr->e_shstrndx == SHN_UNDEF) {
        return NULL;
    }

    const char *strtab = ((const char*) ehdr) + elf_section(ehdr, ehdr->e_shstrndx)->sh_offset;
    return strtab + offset;
}


static void parse_rel_section(const Elf64_Ehdr *eh, const Elf64_Shdr *sh)
{
    size_t nent = sh->sh_size / sh->sh_entsize;

    const Elf64_Rel *rels = (const void*) (((const char*) eh) + sh->sh_offset);

    for (uint32_t i = 0; i < nent; ++i) {
        const Elf64_Rel *rel = &rels[i];
    }
}


static void parse_rela_section(const Elf64_Ehdr *eh, const Elf64_Shdr *sh)
{
}


int elf_load_objfile(struct objfile *objfile)
{
    const Elf64_Ehdr *eh = (const Elf64_Ehdr*) objfile->file_data;

    for (uint64_t shndx = 0; shndx < eh->e_shnum; ++shndx) {
        const Elf64_Shdr *sh = elf_section(eh, shndx);
        const char *name = elf_lookup_str(eh, sh->sh_name);

        switch (sh->sh_type) {
            case SHT_NULL:
                printf("%s null\n", name);
                break;

            case SHT_PROGBITS:
                printf("%s progbits\n", name);
                break;

            case SHT_NOBITS:
                printf("%s nobits\n", name);
                break;

            case SHT_SYMTAB:
                printf("%s symtab\n", name);
                break;

            case SHT_STRTAB:
                printf("%s strtab\n", name);
                break;

            case SHT_DYNSYM:
                printf("%s dynsym\n", name);
                break;

            case SHT_NOTE:
                printf("%s note\n", name);
                break;

            case SHT_REL:
                printf("%s rel\n", name);
                parse_rel_section(eh, sh);
                break;

            case SHT_RELA:
                printf("%s rela\n", name);
                parse_rela_section(eh, sh);
                break;

            default:
                printf("%s unknown section type: %u\n", name,sh->sh_type);
                break;
        }
    }

    return 0;
}


//static int load_vm(struct image *image, const char *path)
//{
//    fprintf(stderr, "Using virtual machine %s\n", path);
//
//    struct archive_file *ar = NULL;
//    int status = open_archive(&ar, path);
//    if (status != 0) {
//        return status;
//    }
//
//    // Load virtual machine from static library and add to image
//    list_for_each_node(objfile, &ar->membs, struct archive_member, listh) {
//        const Elf64_Ehdr *ehdr = objfile->ptr;
//
//        for (uint16_t sh = 0; sh < ehdr->e_shnum; ++sh) {
//            const Elf64_Shdr *shdr = elf_section(ehdr, sh);
//
//            if (shdr->sh_type == SHT_SYMTAB) {
//                size_t nent = shdr->sh_size / shdr->sh_entsize;
//                const Elf64_Sym *symbols = (const Elf64_Sym*) (((const char*) ehdr) + shdr->sh_offset);
//                const char *strtab = ((const char*) ehdr) + elf_section(ehdr, shdr->sh_link)->sh_offset;
//
//                for (uint16_t idx = 0; idx < nent; ++idx) {
//                    const Elf64_Sym *sym = &symbols[idx];
//                    uint8_t type = ELF64_ST_TYPE(sym->st_info);
//                    uint8_t bind = ELF64_ST_BIND(sym->st_info);
//
//                    if (bind == STB_GLOBAL) {
//                        const char *name = strtab + sym->st_name;
//                        const void *p = lookup_global_symbol(ar, name);
//                        printf("global symbol %s %p (type=%u, size=%zu)\n", name, p, type, sym->st_size);
//                    } else if (bind == STB_LOCAL) {
//                        const char *name = strtab + sym->st_name;
//                        printf("local symbol %s (type=%u, size=%zu)\n", name, type, sym->st_size);
//                    } else {
//                        fprintf(stderr, "Unknown binding: %u\n", bind);
//                    }
//                }
//            } else if (shdr->sh_type == SHT_RELA) {
//            } else if (shdr->sh_type == SHT_REL) {
//            } else if (shdr->sh_type == SHT_DYNAMIC) {
//            }
//        }
//    }
//
//    close_archive(&ar);
//
//    return 0;
//}

