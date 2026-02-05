#include "logging.h"
#include "archive_reader.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "utils/endianness.h"


#define AR_MAGIC        "!<arch>\n"
#define AR_MAGIC_SIZE   8
#define AR_END          "`\n"


/* 
 * Archive member header.
 */
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


struct bsd_ranlib_entry_32
{
    uint32_t strx;
    uint32_t offset;
};


struct bsd_ranlib_entry_64
{
    uint64_t strx;
    uint64_t offset;
};


static size_t member_size(const struct ar_header *hdr)
{
    char buffer[11] = {0};
    strncpy(buffer, hdr->size, 10);
    return strtoull(buffer, NULL, 10);
}


static bool check_bsd_name(const struct ar_header *hdr)
{
    return strncmp(hdr->name, "#1/", 3) == 0;
}


static size_t member_name_length(const struct ar_header *hdr, const struct ar_header *strtab)
{
    size_t n;
    char buffer[17] = {0};

    if (check_bsd_name(hdr)) {
        // BSD style extended name
        for (n = 3; n < 16 && hdr->name[n] >= '0' && hdr->name[n] <= '9'; ++n) {
            buffer[n - 3] = hdr->name[n];
        }
        return strtoull(buffer, NULL, 10);

    } else if (hdr->name[0] == '/' && hdr->name[1] >= '0' && hdr->name[1] <= '9') {
        // GNU style extended name
        if (strtab == NULL) {
            return 0;
        }

        memcpy(buffer, &hdr->name[1], 15);
        size_t offset = strtoull(buffer, NULL, 10);

        const char *start = ((const char*) (strtab + 1)) + offset;
        const char *c = start;

        while (*c != '/' && *c != '\0' && *c != '\n') {
            ++c;
        }

        return c - start;
    }

    // Regular name
    for (n = 0; n < 16 && hdr->name[n] != '/' && hdr->name[n] != ' '; ++n);
    return n;
}


static void member_name_string(const struct ar_header *hdr, const struct ar_header *strtab, char *name, size_t maxlen)
{
    size_t len = member_name_length(hdr, strtab);
    if (len == 0) {
        return;
    }

    len = len < maxlen ? len : maxlen - 1;

    if (check_bsd_name(hdr)) {
        // BSD style extended name
        size_t membsz = member_size(hdr);
        if (membsz >= len) {
            memcpy(name, (const void*) (hdr + 1), len);
        }
    } else if (hdr->name[0] == '/' && hdr->name[1] >= '0' && hdr->name[1] <= '9') {
        // GNU style extended name
        if (strtab != NULL) {
            char buffer[17] = {0};
            memcpy(buffer, &hdr->name[1], 15);
            size_t offset = strtoull(buffer, NULL, 10);

            const char *c = ((const char*) (strtab + 1)) + offset;
            memcpy(name, c, len);
        }
    } else {

        // Regular name
        memcpy(name, hdr->name, len);
    }

    if (len < maxlen) {
        name[len] = '\0';
    }
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


static int parse_gnu_ranlib_64(const struct ar_header *hdr, size_t size)
{
    if (size <= sizeof(uint64_t)) {
        return EINVAL;
    }

    const uint64_t *offsets = (const uint64_t*) (hdr + 1);
    uint64_t num_entries = read_be64((const uint8_t*) offsets);

    if ((num_entries + 1) * sizeof(uint64_t) > size) {
        return EINVAL;
    }

    const char *symtab = (const char*) (offsets + num_entries + 1);

    log_debug("Parsing GNU/SysV style ranlib index (%lu symbols)", num_entries);
    for (uint64_t i = 0; i < num_entries; ++i) {
        const char *name = symtab;
        uint64_t offset = read_be64((const uint8_t*) &offsets[i + 1]) + sizeof(struct ar_header);

        log_trace("Symbol '%s' offset %lu", name, offset);

        symtab += strlen(symtab) + 1;
    }
    return 0;
}


static int parse_gnu_ranlib_32(const struct ar_header *hdr, size_t size)
{
    if (size < sizeof(uint32_t)) {
        return EINVAL;
    }

    const uint32_t *offsets = (const uint32_t*) (hdr + 1);
    uint32_t num_entries = read_be32((const uint8_t*) offsets);

    if ((num_entries + 1) * sizeof(uint32_t) > size) {
        return EINVAL;
    }

    const char *symtab = (const char*) (offsets + num_entries + 1);

    log_debug("Parsing GNU/SysV style ranlib index (%lu symbols)", num_entries);
    for (uint32_t i = 0; i < num_entries; ++i) {
        const char *name = symtab;
        uint32_t offset = read_be32((const uint8_t*) &offsets[i + 1]);

        log_trace("Symbol '%s' offset %lu", name, offset);

        symtab += strlen(symtab) + 1;
    }
    return 0;
}


static bool check_gnu_ranlib_32(const struct ar_header *hdr, size_t size)
{
    if (size < sizeof(uint32_t)) {
        return false;
    }

    uint32_t num_entries = read_be32((const uint8_t*) (hdr + 1));
    return (num_entries + 1) * sizeof(uint32_t) <= size;
}


static int parse_bsd_ranlib_64(const struct ar_header *hdr, size_t size, const uint8_t *file_start)
{
    if (size < sizeof(uint64_t)) {
        return EINVAL;
    }

    size_t namelen = member_name_length(hdr, NULL);
    const char *start = (((const char*) (hdr+1)) + namelen);

    if (size < namelen + sizeof(uint64_t)) {
        return EINVAL;
    }

    uint64_t bytes = *((uint64_t*) start);

    if (bytes % sizeof(struct bsd_ranlib_entry_64) != 0) {
        return EINVAL;
    }

    if (size < namelen + 2*sizeof(uint64_t) + bytes) {
        return EINVAL;
    }

    uint64_t num_entries = bytes / sizeof(struct bsd_ranlib_entry_64);
    uint64_t strtabsize = *((uint64_t*) (start + sizeof(uint64_t) + bytes));

    if (size < namelen + 2*sizeof(uint64_t) + bytes + strtabsize) {
        return EINVAL;
    }

    struct bsd_ranlib_entry_64 *entries = (struct bsd_ranlib_entry_64*) (start + sizeof(uint64_t));
    const char *strtab = start + sizeof(uint64_t) + bytes + sizeof(uint64_t);
    
    log_debug("Parsing BSD style ranlib index (%u symbols)", num_entries);

    for (uint64_t i = 0; i < num_entries; ++i) {
        const struct bsd_ranlib_entry_64 *entry = &entries[i];
        const char *name = strtab + entry->strx;
        uint64_t offset = entry->offset;
        offset += sizeof(struct ar_header);
        if (check_bsd_name((struct ar_header*) (file_start + entry->offset))) {
            offset += member_name_length((struct ar_header*) (file_start + entry->offset), NULL);
        }

        log_trace("Symbol '%s' offset %u", name, offset);
    }

    return 0;
}


static int parse_bsd_ranlib_32(const struct ar_header *hdr, size_t size, const uint8_t *file_start)
{
    if (size < sizeof(uint32_t)) {
        return EINVAL;
    }

    size_t namelen = member_name_length(hdr, NULL);
    const char *start = (((const char*) (hdr+1)) + namelen);

    if (size < namelen + sizeof(uint32_t)) {
        return EINVAL;
    }

    size_t bytes = *((uint32_t*) start);

    if (bytes % sizeof(struct bsd_ranlib_entry_32) != 0) {
        return EINVAL;
    }

    if (size < namelen + 2*sizeof(uint32_t) + bytes) {
        return EINVAL;
    }

    uint32_t num_entries = bytes / sizeof(struct bsd_ranlib_entry_32);
    uint32_t strtabsize = *((uint32_t*) (start + sizeof(uint32_t) + bytes));

    if (size < namelen + 2*sizeof(uint32_t) + bytes + strtabsize) {
        return EINVAL;
    }

    struct bsd_ranlib_entry_32 *entries = (struct bsd_ranlib_entry_32*) (start + sizeof(uint32_t));
    const char *strtab = start + sizeof(uint32_t) + bytes + sizeof(uint32_t);

    log_debug("Parsing BSD style ranlib index (%u symbols)", num_entries);

    for (uint32_t i = 0; i < num_entries; ++i) {
        const struct bsd_ranlib_entry_32 *entry = &entries[i];
        const char *name = strtab + entry->strx;
        uint32_t offset = entry->offset;
        offset += sizeof(struct ar_header);
        if (check_bsd_name((struct ar_header*) (file_start + entry->offset))) {
            offset += member_name_length((struct ar_header*) (file_start + entry->offset), NULL);
        }

        log_trace("Symbol '%s' offset %u", name, offset);
    }

    return 0;
}


static int parse_file(const uint8_t *ptr, size_t size, 
                      struct archive *archive, struct archives *index)
{
    size_t offset = AR_MAGIC_SIZE;

    const struct ar_header *strtab = NULL;
    const struct ar_header *ranlib = NULL;

    while (offset < size) {
        const struct ar_header *hdr = (const void*) (ptr + offset);
        size_t membsz = member_size(hdr);

        if (size - offset < sizeof(*hdr)) {
            log_fatal("Unexpected end of archive");
            return EBADF;
        }

        if (strncmp(hdr->end, AR_END, 2) != 0) {
            log_fatal("Invalid archive member header");
            return EBADF;
        }

        if (hdr->name[0] == '/' && (hdr->name[1] == ' ' || hdr->name[1] == '\0')) {
            log_trace("Found GNU/SysV style ranlib symbol index at offset %zu", offset);

            if (ranlib == NULL) {
                ranlib = hdr;
            } else {
                log_warning("Multiple ranlib symbol indexes detected");
            }

        } else if (strncmp(hdr->name, "/SYM64/", 7) == 0) {
            log_trace("Found GNU/SysV style ranlib symbol index at offset %zu", offset);

            if (ranlib == NULL) {
                ranlib = hdr;
            } else {
                log_warning("Multiple ranlib symbol indexes detected");
            }
        
        } else if (strncmp(hdr->name, "//", 2) == 0) {
            log_trace("Found GNU/SysV style extended string table at offset %zu", offset);

            if (strtab == NULL) {
                strtab = hdr;
            } else {
                log_warning("Multiple extended string tables detected");
            }

        } else {
            size_t len = member_name_length(hdr, strtab);
            char name[len + 1];
            member_name_string(hdr, strtab, name, len+1);

            bool is_bsd = check_bsd_name(hdr);

            if (is_bsd && strcmp(name, "__.SYMDEF_64") == 0) {
                log_trace("Found BSD style ranlib symbol index at offset %zu", offset);
                
                if (ranlib == NULL) {
                    ranlib = hdr;
                } else {
                    log_warning("Multiple ranlib symbol indexes detected");
                }

            } else if (is_bsd && strcmp(name, "__.SYMDEF") == 0) {
                log_trace("Found BSD style ranlib symbol index at offset %zu", offset);

                if (ranlib == NULL) {
                    ranlib = hdr;
                } else {
                    log_warning("Multiple ranlib symbol indexes detected");
                }
            } else {
                // Regular member
                archive_add_member(archive, name, offset + sizeof(*hdr), membsz);
            }
        }

        offset += sizeof(*hdr) + membsz;
        offset += offset % 2; // offsets must be-2 byte aligned
    }

    if (ranlib == NULL) {
        log_warning("Archive has no ranlib symbol index");
        return 0;
    }

    size_t ransize = member_size(ranlib);

    if (check_bsd_name(ranlib)) {
        size_t len = member_name_length(ranlib, NULL);
        char name[len + 1];
        member_name_string(ranlib, NULL, name, len+1);

        if (strcmp(name, "__.SYMDEF_64") == 0) {
            parse_bsd_ranlib_64(ranlib, ransize, ptr);
        } else {
            parse_bsd_ranlib_32(ranlib, ransize, ptr);
        }

    } else {
        if (check_gnu_ranlib_32(ranlib, ransize)) {
            parse_gnu_ranlib_32(ranlib, ransize);
        } else {
            parse_gnu_ranlib_64(ranlib, ransize);
        }
    }

//    const uint32_t *offsets = (const uint32_t*) (ranlib + 1);
//    uint32_t num_entries = bswap_32(offsets[0]);
//    const char *symtab = (const char*) (offsets + num_entries + 1);
//
//    log_trace("Parsing ranlib index (%u symbols)", num_entries);
//    for (uint32_t i = 0; i < num_entries; ++i) {
//        uint32_t offset = bswap_32(offsets[i + 1]) + sizeof(struct ar_header);
//        struct archive_member *member = archive_get_member(archive, offset);
//
//        if (member != NULL) {
//            archives_insert_symbol(index, member, symtab);
//        } else {
//            log_error("Symbol '%s' (index %u) refers to non-existing archive member (offset %lu)",
//                    symtab, i, offset);
//        }
//        symtab += strlen(symtab) + 1;
//    }

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
