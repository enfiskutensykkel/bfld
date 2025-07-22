#include "ar.h"
#include "elf.h"
#include "image.h"
#include <utils/list.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <elf.h>
#include <getopt.h>
#include <errno.h>


static int load_vm(struct image *image, const char *path)
{
    fprintf(stderr, "Using virtual machine %s\n", path);

    struct archive_file *ar = NULL;
    int status = open_archive(&ar, path);
    if (status != 0) {
        return status;
    }

    // Load virtual machine from static library and add to image
    list_for_each_node(objfile, &ar->membs, struct archive_member, listh) {
        const Elf64_Ehdr *ehdr = objfile->ptr;

        for (uint16_t sh = 0; sh < ehdr->e_shnum; ++sh) {
            const Elf64_Shdr *shdr = elf_section(ehdr, sh);

            if (shdr->sh_type == SHT_SYMTAB) {
                size_t nent = shdr->sh_size / shdr->sh_entsize;
                const Elf64_Sym *symbols = (const Elf64_Sym*) (((const char*) ehdr) + shdr->sh_offset);
                const char *strtab = ((const char*) ehdr) + elf_section(ehdr, shdr->sh_link)->sh_offset;

                for (uint16_t idx = 0; idx < nent; ++idx) {
                    const Elf64_Sym *sym = &symbols[idx];
                    uint8_t type = ELF64_ST_TYPE(sym->st_info);
                    uint8_t bind = ELF64_ST_BIND(sym->st_info);

                    if (bind == STB_GLOBAL) {
                        const char *name = strtab + sym->st_name;
                        const void *p = lookup_global_symbol(ar, name);
                        printf("global symbol %s %p (type=%u, size=%zu)\n", name, p, type, sym->st_size);
                    } else if (bind == STB_LOCAL) {
                        const char *name = strtab + sym->st_name;
                        printf("local symbol %s (type=%u, size=%zu)\n", name, type, sym->st_size);
                    } else {
                        fprintf(stderr, "Unknown binding: %u\n", bind);
                    }
                }
            } else if (shdr->sh_type == SHT_RELA) {
            } else if (shdr->sh_type == SHT_REL) {
            } else if (shdr->sh_type == SHT_DYNAMIC) {
            }
        }
    }

    close_archive(&ar);

    return 0;
}



int main(int argc, char **argv)
{
    int c;
    int idx = 0;

    static struct option options[] = {
        {"vm", required_argument, 0, 'i'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    const char *vmpath = DEFAULT_BFVM;

    while ((c = getopt_long_only(argc, argv, ":h", options, &idx)) != -1) {
        switch (c) {
            case 'i':
                vmpath = optarg;
                break;

            case 'h':
                fprintf(stdout, "Usage: %s [--vm VM_FILE] BFO_FILE\n", argv[0]);
                exit(0);

            case ':':
                fprintf(stderr, "Missing value for option %s\n", argv[optind-1]);
                exit(1);

            default:
                fprintf(stderr, "Unknown option %s\n", argv[optind-1]);
                exit(1);
        }
    }

//    if (optind >= argc) {
//        fprintf(stderr, "Missing argument FILE\n");
//        exit(1);
//    }

    
    struct image *img = NULL;
    int status = create_image(&img);
    if (status != 0) {
        exit(2);
    }

    load_vm(img, vmpath);

    for (int i = optind; i < argc; ++i) {
        fprintf(stderr, "Parsing file: %s\n", argv[i]);
    }

    destroy_image(&img);
    exit(0);
}
