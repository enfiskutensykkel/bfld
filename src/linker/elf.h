#ifndef __BFLD_ELF_FILE_H__
#define __BFLD_ELF_FILE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <elf.h>


bool check_elf_header(const Elf64_Ehdr *ehdr);


const Elf64_Shdr * elf_section(const Elf64_Ehdr *ehdr, uint16_t idx);


const char * get_elf_string(const Elf64_Ehdr *ehdr, uint16_t sect, uint64_t offset);


#define lookup_string(ehdr, offset) \
    ((ehdr)->e_shstrndx == SHN_UNDEF) ? NULL : get_elf_string((ehdr), (ehdr)->e_shstrndx, (offset))


#ifdef __cplusplus
}
#endif
#endif
