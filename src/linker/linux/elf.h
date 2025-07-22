#ifndef __BFLD_ELF_FILE_H__
#define __BFLD_ELF_FILE_H__

#include "../inputobj.h"
#include <stdbool.h>
#include <stdint.h>
#include <elf.h>


/*
 * Check if the first bytes contain the magic ELF64 signature.
 */
bool elf_check_magic(const void *file_start);


/*
 * Get the specified ELF section header.
 */
const Elf64_Shdr * elf_section(const Elf64_Ehdr *ehdr, uint16_t idx);


/*
 * Look up a string from the string table (if present).
 */
const char * elf_lookup_str(const Elf64_Ehdr *ehdr, uint32_t offset);


/*
 * Parse the ELF object file and set members on the input object file.
 */
int elf_load_objfile(struct input_objfile *objfile);


const Elf64_Shdr * elf_lookup_section(const struct input_sect *sect);


#endif
