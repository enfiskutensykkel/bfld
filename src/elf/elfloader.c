#include "objfile_loader.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <elf.h>
#include <errno.h>
#include <string.h>


struct context
{
    const Elf64_Ehdr *eh;
};


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

    return true;
}


static int parse_elf_file(void **ctx_data, const char *filename, const uint8_t *data, size_t size)
{
    struct context *ctx = malloc(sizeof(struct context));
    if (ctx == NULL) {
        return ENOMEM;
    }

    ctx->eh = (const Elf64_Ehdr*) data;

    *ctx_data = ctx;
    return 0;
}


const struct objfile_loader_ops elf_loader_ops = {
    .probe = check_elf_header,
    .parse_file = parse_elf_file,
    .release = free,
};



OBJFILE_LOADER_INIT static void elf_loader_init(void)
{
    objfile_loader_register("elf", &elf_loader_ops);
}
