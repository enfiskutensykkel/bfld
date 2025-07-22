#include "elf.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>


bool check_elf_header(const Elf64_Ehdr *ehdr)
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


const Elf64_Shdr * elf_section(const Elf64_Ehdr *ehdr, uint16_t idx)
{
    return ((const Elf64_Shdr*) (((const char*) ehdr) + ehdr->e_shoff)) + idx;
}


const char * get_elf_string(const Elf64_Ehdr *ehdr, uint16_t sect, uint64_t offset)
{
    if (sect == SHN_UNDEF) {
        return NULL;
    }

    const char *strtab = ((const char*) ehdr) + elf_section(ehdr, sect)->sh_offset;
    return strtab + offset;
}
