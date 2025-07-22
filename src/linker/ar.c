#include "ar.h"
#include "elf.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
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


const void * lookup_global_symbol(const struct archive_file *ar, const char *name)
{
    size_t offset = AR_MAGIC_SIZE;
    size_t size = ar->size;
    const void *ptr = ar->ptr;

    while (offset < size) {
        const struct ar_header *hdr = (const void*) (((const char*) ptr) + offset);
        size_t membsz = get_member_size(hdr);

        if (strncmp(hdr->name, "/ ", 2) == 0 && membsz >= sizeof(uint32_t)) {
            const uint32_t *offsets = (const uint32_t*) (hdr + 1);
            uint32_t num_entries = ntohl(offsets[0]);
            const char *symtab = (const char*) (offsets + num_entries + 1);

            for (uint32_t i = 0; i < num_entries; ++i) {
                if (strcmp(symtab, name) == 0) {
                    uint32_t offset = ntohl(offsets[i + 1]);
                    return (const void*) (((const char*) ar->ptr) + offset);
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


int lookup_archive_member_name(char **name, const struct archive_member *armemb)
{
    size_t offset = AR_MAGIC_SIZE;
    const struct archive_file *ar = armemb->ar;
    size_t size = ar->size;
    const void *ptr = ar->ptr;
    
    const char *long_names = NULL;
    *name = NULL;

    while (offset < size) {
        const struct ar_header *hdr = (const void*) (((const char*) ptr) + offset);
        size_t membsz = get_member_size(hdr);

        if (strncmp(hdr->name, "//", 2) == 0) {
            // Extended names references
            long_names = (const char*) (hdr + 1);
        } else if (armemb->ptr == (const void*) (hdr + 1)) {
            return get_member_name(hdr, long_names, name);
        }

        offset += sizeof(*hdr) + membsz;
    }

    return ENODATA;
}


int open_archive(struct archive_file **ar, const char *path)
{
    *ar = NULL;

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Could not open file %s: %s\n", path, strerror(errno));
        return EBADF;
    }

    // Get the file size so we can memory-map the entire thing
    struct stat stat;
    if (fstat(fd, &stat) == -1) {
        close(fd);
        fprintf(stderr, "Could not get file size for file %s: %s\n", path, strerror(errno));
        return EBADF;
    }
    size_t size = stat.st_size;
    
    const void *ptr = mmap(NULL, size, PROT_READ, MAP_SHARED | MAP_POPULATE, fd, 0);
    if (ptr == MAP_FAILED) {
        close(fd);
        fprintf(stderr, "Could not memory map file %s: %s\n", path, strerror(errno));
        return EBADF;
    }

    // FIXME: Create "virtual" archive if ELF64 header

    if (strncmp(ptr, AR_MAGIC, AR_MAGIC_SIZE) != 0) {
        munmap((void*) ptr, size);
        close(fd);
        fprintf(stderr, "Corrupt static library %s: Invalid signature\n", path);
        return EBADF;
    }
    size_t offset = AR_MAGIC_SIZE;

    struct archive_file *archive = malloc(sizeof(struct archive_file));
    if (archive == NULL) {
        munmap((void*) ptr, size);
        close(fd);
        return ENOMEM;
    }
    archive->fd = fd;
    archive->ptr = ptr;
    archive->size = size;
    archive->nmembs = 0;
    list_head_init(&archive->membs);

    const char *long_names = NULL;

    while (offset < size) {
        const struct ar_header *hdr = (const void*) (((const char*) ptr) + offset);
        size_t membsz = get_member_size(hdr);

        if (size - offset < sizeof(*hdr)) {
            fprintf(stderr, "Corrupt static library %s: Unexpected end of file\n", path);
            close_archive(&archive);
            return EBADF;
        }

        if (strncmp(hdr->end, AR_END, 2) != 0) {
            fprintf(stderr, "Corrupt static library %s: Invalid member header\n", path);
            close_archive(&archive);
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
        } else if (check_elf_header((const Elf64_Ehdr*) (hdr + 1))) {
            struct archive_member *memb = malloc(sizeof(struct archive_member));
            if (memb == NULL) {
                close_archive(&archive);
                return ENOMEM;
            }

            memb->ar = archive;
            memb->size = membsz;
            memb->offs = offset + sizeof(*hdr);
            memb->ptr = (const void*) (hdr + 1);

            archive->nmembs++;
            list_append_entry(&archive->membs, &memb->listh);
        } else {
            char *name = NULL;
            get_member_name(hdr, long_names, &name);
            fprintf(stderr, "Ignoring non-ELF archive member %s in %s\n", 
                    name, path);
            free(name);
        }

        offset += sizeof(*hdr) + membsz;
    }

    *ar = archive;
    return 0;
}


void close_archive(struct archive_file **ar)
{
    if (*ar != NULL) {
        list_for_each_node(it, &(*ar)->membs, struct archive_member, listh) {
            list_remove_entry(&it->listh);
            free(it);
        }

        munmap((void*) (*ar)->ptr, (*ar)->size);
        close((*ar)->fd);
        free(*ar);
        *ar = NULL;
    }
}
