#include "logging.h"
#include "archive_reader.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <byteswap.h>

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


static size_t get_member_size(const struct ar_header *hdr)
{
    char buffer[11];
    memset(buffer, 0, 11);
    strncpy(buffer, hdr->size, 10);
    return strtoull(buffer, NULL, 10);
}


static int get_member_name(const struct ar_header *hdr, const struct ar_header *strtab, char **name)
{
    // TODO: deal with BSD style extended names if (strncmp(hdr->name, "#1/", 3) == 0) 
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


static int parse_file(const uint8_t *ptr, size_t size, 
                      struct archive *archive, struct archives *index)
{
    size_t offset = AR_MAGIC_SIZE;

    const struct ar_header *strtab = NULL;
    const struct ar_header *ranlib = NULL;

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

        if (hdr->name[0] == '/' && (hdr->name[1] == ' ' || hdr->name[1] == '\0')) {
            log_trace("Found GNU/SysV ranlib index at offset %zu", offset);
            ranlib = hdr;
        } else if (strncmp(hdr->name, "__.SYMDEF", 9) == 0) {
            log_notice("Found BSD-style ranlib index at offset %zu", offset);
            ranlib = hdr;
        } else if (strncmp(hdr->name, "/SYM64/", 7) == 0) {
            log_fatal("SYM64 archive format not supported");
            return ENOTSUP;
        } else if (strncmp(hdr->name, "//", 2) == 0) {
            log_trace("Found GNU-style long name string table at offset %zu", offset);
            strtab = hdr;
        } else {
            if (strncmp(hdr->name, "#1/", 3) == 0) {
                log_fatal("BSD-style archives are not supported");
                return ENOTSUP;
            }
            char *name = NULL;
            get_member_name(hdr, strtab, &name);

            if (name == NULL) {
                log_notice("Unable extract name for member '%s' with size %zu at offset %zu", name, membsz, offset);
            }

            archive_add_member(archive, name, offset + sizeof(*hdr), membsz);

            if (name != NULL) {
                free(name);
            }
        }

        offset += sizeof(*hdr) + membsz;
        offset += offset % 2; // offsets must be-2 byte aligned
    }

    // Parse ranlib index
    if (ranlib == NULL) {
        log_warning("Archive has no ranlib index and will be ignored");
        return 0;
    }

    const uint32_t *offsets = (const uint32_t*) (ranlib + 1);
    uint32_t num_entries = bswap_32(offsets[0]);
    const char *symtab = (const char*) (offsets + num_entries + 1);

    log_trace("Parsing ranlib index (%u symbols)", num_entries);
    for (uint32_t i = 0; i < num_entries; ++i) {
        uint32_t offset = bswap_32(offsets[i + 1]) + sizeof(struct ar_header);
        struct archive_member *member = archive_get_member(archive, offset);

        if (member != NULL) {
            archives_insert_symbol(index, member, symtab);
        } else {
            log_error("Symbol '%s' (index %u) refers to non-existing archive member (offset %lu)",
                    symtab, i, offset);
        }
        symtab += strlen(symtab) + 1;
    }

    return 0;
}


const struct archive_reader linux_ar_fe = {
    .name = "ar",
    .probe_file = check_magic,
    .parse_file = parse_file,
};


__attribute__((constructor))
static void linux_ar_reader_init(void)
{
    archive_reader_register(&linux_ar_fe);
}
