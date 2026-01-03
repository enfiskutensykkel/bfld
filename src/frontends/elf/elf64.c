#include <objfile_frontend.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <objfile.h>
#include <secttab.h>
#include <symtab.h>


static bool check_elf_header(const uint8_t *file_data, size_t file_size)
{
    return false;
}


static int parse_elf_file(const uint8_t *file_data, 
                          size_t file_size,
                          struct objfile *objfile, 
                          struct secttab *sections, 
                          struct symtab *symbols)
{
    return 1;
}


const struct objfile_frontend elf64_fe = {
    .name = "ELF64",
    .probe_file = check_elf_header,
    .parse_file = parse_elf_file,
};


__attribute__((constructor))
static void elf_fe_init(void)
{
    objfile_frontend_register(&elf64_fe);
}
