#ifndef __BFLD_ARCHIVE_FILE_H__
#define __BFLD_ARCHIVE_FILE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <utils/list.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


struct archive_file;


struct archive_member
{
    const struct archive_file *ar;
    struct list_head listh;
    const void *ptr;
    uint64_t offs;
    size_t size;
};


struct archive_file
{
    int fd;
    size_t size;
    const void *ptr;
    size_t nmembs;
    struct list_head membs;
};


int lookup_archive_member_name(char **name, const struct archive_member *armemb);


const void * lookup_global_symbol(const struct archive_file *ar, const char *name);


int open_archive(struct archive_file **ar, const char *path);


void close_archive(struct archive_file **ar);


#ifdef __cplusplus
}
#endif
#endif
