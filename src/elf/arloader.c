#include "logging.h"
#include "archive_loader.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>


#define AR_MAGIC        "!<arch>\n"
#define AR_MAGIC_SIZE   8
#define AR_END          "`\n"


/*
 * Reuse the ELF loader in order to parse individual members.
 */
extern const struct objfile_loader elf_loader;


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


/*
 * Hold some data about the archive file.
 */
struct ar_file
{
    const uint8_t *start;
    size_t size;
    const struct ar_header *strtab;
    const struct ar_header *ranlib;
    size_t num_members;
};


static size_t get_member_size(const struct ar_header *hdr)
{
    char buffer[11];
    memset(buffer, 0, 11);
    strncpy(buffer, hdr->size, 10);
    return strtoull(buffer, NULL, 10);
}


static int get_member_name(const struct ar_header *hdr, const struct ar_header *strtab, char **name)
{
    size_t len = 0;
    for (; len < 16 && hdr->name[len] != '/'; ++len);

    *name = NULL;

    const char *long_names = NULL;
    if (strtab != NULL) {
        long_names = (const char*) (strtab + 1);
    }

    if (long_names != NULL && len == 0) {
        char buffer[16];
        memset(buffer, 0, 16);
        strncpy(buffer, &hdr->name[1], 15);

        size_t offset = strtoull(buffer, NULL, 10);
        for (; long_names[offset + len] != '/'; ++len);

        *name = malloc(len + 1);
        if (*name == NULL) {
            return ENOMEM;
        }
        strncpy(*name, &long_names[offset], len);
        (*name)[len] = '\0';

    } else if (len > 0) {
        *name = malloc(len + 1);
        if (*name == NULL) {
            return ENOMEM;
        }
        strncpy(*name, hdr->name, len);
        (*name)[len] = '\0';

    } else {
        *name = malloc(1);
        if (*name == NULL) {
            return ENOMEM;
        }
        (*name)[0] = '\0';
    }

    return 0;
}


static bool check_magic(const uint8_t *ptr, size_t size)
{
    if (size <= sizeof(struct ar_header)) {
        return false;
    }

    if (strncmp((const char*) ptr, AR_MAGIC, AR_MAGIC_SIZE) != 0) {
        return false;
    }

    return true;
}


static int init(void **ctx, const uint8_t *ptr, size_t size)
{
    struct ar_file *ctx_data = malloc(sizeof(struct ar_file));
    if (ctx_data == NULL) {
        return ENOMEM;
    }

    ctx_data->start = ptr;
    ctx_data->size = size;
    ctx_data->num_members = 0;
    ctx_data->strtab = NULL;
    ctx_data->ranlib = NULL;

    *ctx = ctx_data;
    return 0;
}


static int parse_ranlib_index(void *ctxptr, bool (*emit)(void *user, const char*, uint64_t), void *user)
{
    struct ar_file *ctx = ctxptr;

    if (ctx->ranlib == NULL) {
        log_fatal("Archive has no symbol index");
        return EBADF;
    }

    const uint32_t *offsets = (const uint32_t*) (ctx->ranlib + 1);
    uint32_t num_entries = ntohl(offsets[0]);
    const char *symtab = (const char*) (offsets + num_entries + 1);

    for (uint32_t i = 0; i < num_entries; ++i) {
        emit(user, symtab, ntohl(offsets[i + 1]));
        symtab += strlen(symtab) + 1;
    }

    return 0;
}


static int parse_members(void *ctx, bool (*emit)(void *user, uint64_t, const char*, size_t, size_t), void *user)
{
    struct ar_file *ctx_data = ctx;
    const uint8_t *ptr = ctx_data->start;
    size_t size = ctx_data->size;

    size_t offset = AR_MAGIC_SIZE;
    
    while (offset < size) {
        const struct ar_header *hdr = (const void*) (ptr + offset);
        size_t membsz = get_member_size(hdr);

        if (size - offset < sizeof(*hdr)) {
            log_fatal("Unexpected end of archive");
            return EBADF;
        }

        if (strncmp(hdr->end, AR_END, 2) != 0) {
            log_fatal("Invalid archive member header");
            return EBADF;
        }

        if (strncmp(hdr->name, "/ ", 2) == 0) {
            ctx_data->ranlib = hdr;

        } else if (strncmp(hdr->name, "/SYM64/", 7) == 0) {
            log_fatal("SYM64 archive format not supported");
            return ENOTSUP;

        } else if (strncmp(hdr->name, "//", 2) == 0) {
            ctx_data->strtab = hdr;

        } else {
            char *name = NULL;
            get_member_name(hdr, ctx_data->strtab, &name);
            emit(user, offset, name, offset + sizeof(struct ar_header), size);
            if (name != NULL) {
                free(name);
            }
        }

        offset += sizeof(*hdr) + membsz;
    }

    return 0;
}


const struct archive_loader ar_loader = {
    .name = "ar-loader",
    .member_loader = &elf_loader,
    .probe = check_magic,
    .parse_file = init,
    .parse_members = parse_members,
    .parse_symbol_index = parse_ranlib_index,
    .release = free,
};


ARCHIVE_LOADER_INIT static void ar_loader_init(void)
{
    archive_loader_register(&ar_loader);
}
