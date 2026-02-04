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
        // GNU style long name
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
        memcpy(name, (const void*) (hdr + 1), len);
    } else if (hdr->name[0] == '/' && hdr->name[1] >= '0' && hdr->name[1] <= '9') {
        if (strtab != NULL) {
            char buffer[17] = {0};
            memcpy(buffer, &hdr->name[1], 15);
            size_t offset = strtoull(buffer, NULL, 10);

            const char *c = ((const char*) (strtab + 1)) + offset;
            memcpy(name, c, len);
        }
    } else {
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


static int parse_gnu_ranlib(const struct ar_header *hdr)
{
    size_t size = member_size(hdr);
    uint64_t num_entries = read_be32(((const uint8_t*) (hdr + 1)));
    const uint32_t *dword_offs = NULL;
    const uint64_t *qword_offs = NULL;
    const char *symtab = NULL;

    if (num_entries * 4 + 4 < size) {
        dword_offs = (const uint32_t*) (hdr + 1);
        symtab = (const char*) (dword_offs + num_entries + 1);
        log_debug("Parsing GNU/SysV style ranlib index (%lu symbols)", num_entries);
    } else {
        num_entries = read_be64(((const uint8_t*) (hdr + 1)));
        qword_offs = (const uint64_t*) (hdr + 1);
        symtab = (const char*) (qword_offs + num_entries + 1);
        log_notice("GNU/SysV style ranlib index appears to be 64-bit (%lu symbols)", num_entries);
    }

    for (uint64_t i = 0; i < num_entries; ++i) {
        uint64_t offset;

        if (qword_offs != NULL) {
            offset = read_be64((const uint8_t*) &qword_offs[i + 1]) + sizeof(struct ar_header);
        } else {
            offset = read_be32((const uint8_t*) &dword_offs[i + 1]) + sizeof(struct ar_header);
        }

        log_trace("Symbol '%s' offset %lu", symtab, offset);

        symtab += strlen(symtab) + 1;
    }
    return 0;
}


static int parse_bsd_ranlib_64(const struct ar_header *hdr)
{
    const char *start = (((const char*) (hdr+1)) + member_name_length(hdr, NULL));
    uint64_t size = *((uint64_t*) start);
    uint64_t num_entries = size / sizeof(struct bsd_ranlib_entry_64);
    uint64_t strtabsize = *((uint64_t*) (start + size));

    struct bsd_ranlib_entry_64 *entries = (struct bsd_ranlib_entry_64*) (start + sizeof(uint64_t));
    const char *strtab = start + sizeof(uint64_t) + size + sizeof(uint64_t);
    
    log_debug("Parsing BSD style ranlib index (%u symbols)", num_entries);

    for (uint64_t i = 0; i < num_entries; ++i) {
        const struct bsd_ranlib_entry_64 *entry = &entries[i];
        const char *name = strtab + entry->strx;

        log_trace("Symbol '%s' offset %u", name, entry->offset);
    }

    return 0;
}


static int parse_bsd_ranlib_32(const struct ar_header *hdr)
{
    const char *start = (((const char*) (hdr+1)) + member_name_length(hdr, NULL));
    size_t size = *((uint32_t*) start);
    uint32_t num_entries = size / sizeof(struct bsd_ranlib_entry_32);
    uint32_t strtabsize = *((uint32_t*) (start + size));

    struct bsd_ranlib_entry_32 *entries = (struct bsd_ranlib_entry_32*) (start + sizeof(uint32_t));
    const char *strtab = start + sizeof(uint32_t) + size + sizeof(uint32_t);
    
    log_debug("Parsing BSD style ranlib index (%u symbols)", num_entries);

    for (uint32_t i = 0; i < num_entries; ++i) {
        const struct bsd_ranlib_entry_32 *entry = &entries[i];
        const char *name = strtab + entry->strx;

        log_trace("Symbol '%s' offset %u", name, entry->offset);
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
            log_trace("Found GNU/SysV ranlib index at offset %zu", offset);

            if (ranlib == NULL) {
                parse_gnu_ranlib(hdr);
                ranlib = hdr;
            }
        } else if (strncmp(hdr->name, "/SYM64/", 7) == 0) {
        
        } else if (strncmp(hdr->name, "/SYM/", 5) == 0) {

        } else if (strncmp(hdr->name, "//", 2) == 0) {
            log_trace("Found GNU/SysV style long name string table at offset %zu", offset);
            strtab = hdr;

        } else {
            size_t len = member_name_length(hdr, strtab);
            char name[len + 1];
            member_name_string(hdr, strtab, name, len+1);

            bool is_bsd = check_bsd_name(hdr);

            if (is_bsd && strcmp(name, "__.SYMDEF_64") == 0) {
                log_trace("Found BSD-style ranlib index at offset %zu", offset);
                
                if (ranlib == NULL) {
                    parse_bsd_ranlib_64(hdr);
                    ranlib = hdr;
                }

            } else if (is_bsd && strcmp(name, "__.SYMDEF") == 0) {
                log_trace("Found BSD-style ranlib index at offset %zu", offset);

                if (ranlib == NULL) {
                    parse_bsd_ranlib_32(hdr);
                    ranlib = hdr;
                }
            } 

            //archive_add_member(archive, name, offset + sizeof(*hdr), membsz);
        }

        offset += sizeof(*hdr) + membsz;
        offset += offset % 2; // offsets must be-2 byte aligned
    }

    // Parse ranlib index
    if (ranlib == NULL) {
        log_warning("Archive has no ranlib index and will be ignored");
        return 0;
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
