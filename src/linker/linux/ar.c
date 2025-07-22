#include "ar.h"
#include "../inputobj.h"
#include "../io.h"
#include "elf.h"
#include <utils/list.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>


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


static bool ar_check_magic(const void *contents)
{
    if (strncmp(contents, AR_MAGIC, AR_MAGIC_SIZE) != 0) {
        return false;
    }

    return true;
}


const void * ar_lookup_gsym(const struct ifile *fp, const char *symname)
{
    if (!ar_check_magic(fp->ptr)) {
        return NULL;
    }

    size_t offset = AR_MAGIC_SIZE;
    size_t size = fp->size;
    const void *ptr = fp->ptr;

    while (offset < size) {
        const struct ar_header *hdr = (const void*) (((const char*) ptr) + offset);
        size_t membsz = get_member_size(hdr);

        if (strncmp(hdr->name, "/ ", 2) == 0 && membsz >= sizeof(uint32_t)) {
            const uint32_t *offsets = (const uint32_t*) (hdr + 1);
            uint32_t num_entries = ntohl(offsets[0]);
            const char *symtab = (const char*) (offsets + num_entries + 1);

            for (uint32_t i = 0; i < num_entries; ++i) {
                if (strcmp(symtab, symname) == 0) {
                    uint32_t offset = ntohl(offsets[i + 1]);
                    return (const void*) (((const char*) fp->ptr) + offset);
                }

                symtab += strlen(symtab) + 1;
            }

        } else if (strncmp(hdr->name, "/SYM64/", 7) == 0) {
            // FIXME: implement this
        }

        offset += sizeof(*hdr) + membsz;
    }

    return NULL;
}


static int get_member_name(const struct ar_header *hdr, const char *long_names, char **name)
{
    size_t len = 0;
    for (; len < 16 && hdr->name[len] != '/'; ++len);

    *name = NULL;

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


static int read_members(const struct ifile *fp, struct list_head *objfiles)
{
    size_t offset = AR_MAGIC_SIZE;
    const void *ptr = fp->ptr;
    size_t size = fp->size;

    const char *long_names = NULL;

    while (offset < size) {
        const struct ar_header *hdr = (const void*) (((const char*) ptr) + offset);
        size_t membsz = get_member_size(hdr);

        if (size - offset < sizeof(*hdr)) {
            fprintf(stderr, "Unexpected end of file\n");
            input_objfile_put_all(objfiles);
            return EBADF;
        }

        if (strncmp(hdr->end, AR_END, 2) != 0) {
            fprintf(stderr, "Invalid archive member header\n");
            input_objfile_put_all(objfiles);
            return EBADF;
        }

        if (strncmp(hdr->name, "/ ", 2) == 0) {
            // Archive index containing global symbols
            // Do nothing for now.
        } else if (strncmp(hdr->name, "/SYM64/", 7) == 0) {
            // Archive index containing global symbols
            // Do nothing for now.
        } else if (strncmp(hdr->name, "//", 2) == 0) {
            // Extended names references
            long_names = (const char*) (hdr + 1);

        } else if (elf_check_magic((const Elf64_Ehdr*) (hdr + 1))) {

            struct input_objfile *obj = input_objfile_alloc((const void*) (hdr+1), membsz);
            if (obj == NULL) {
                input_objfile_put_all(objfiles);
                return ENOMEM;
            }

            int status = elf_load_objfile(obj);
            if (status != 0) {
                input_objfile_free(obj);
                return status;
            }

            list_append_entry(objfiles, &obj->entry);

        } else {
            char *name = NULL;
            get_member_name(hdr, long_names, &name);
            fprintf(stderr, "Ignoring non-ELF archive member %s\n", name);
            free(name);
        }

        offset += sizeof(*hdr) + membsz;
    }

    return 0;
}


int input_objfile_get_all(const struct ifile *fp, struct list_head *objfiles)
{
    if (ar_check_magic(fp->ptr)) {
        return read_members(fp, objfiles);

    } else if (elf_check_magic(fp->ptr)) {
        struct input_objfile *obj = input_objfile_alloc(fp->ptr, fp->size);
        if (obj == NULL) {
            return ENOMEM;
        }

        int status = elf_load_objfile(obj);
        if (status != 0) {
            input_objfile_free(obj);
            return status;
        }

        list_append_entry(objfiles, &obj->entry);
        return 0;
    }

    fprintf(stderr, "Unrecognized file format\n");
    return EBADF;
}
