#include "bfld_ar.h"
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


#define AR_MAGIC        "!<arch>\n"
#define AR_MAGIC_SIZE   8
#define AR_END          "`\n"


struct ar_hdr
{
    char name[16];  // Member file name
    char date[12];  // File date, decimal seconds since UNIX Epoch
    char uid[6];    // User ID in ASCII decimal
    char gid[6];    // Group ID, in ASCII decimal
    char mode[8];   // File mode, in ASCII octal
    char size[10];  // File size, in ASCII decimal
    char end[2];    // Always contains AR_END
};


int bfld_read_archive(FILE *fp, struct bfld_archive **archive)
{
    *archive = NULL;

    char magic[AR_MAGIC_SIZE];
    if (fread(magic, AR_MAGIC_SIZE, 1, fp) <= 0) {
        return EINVAL;
    } else if (strncmp(magic, AR_MAGIC, AR_MAGIC_SIZE) != 0) {
        return EINVAL;
    }

    struct bfld_archive *ar = malloc(sizeof(struct bfld_archive));
    if (ar == NULL) {
        return ENOMEM;
    }

    bfld_list_init(&ar->members);

    while (true) {
        struct ar_hdr hdr;

        if (fread(&hdr, sizeof(hdr), 1, fp) <= 0) {
            if (feof(fp)) {
                break;
            }
            bfld_free_archive(&ar);
            return EINVAL;
        } else if (strncmp(hdr.end, AR_END, 2) != 0) {
            bfld_free_archive(&ar);
            return EINVAL;
        }

        char buffer[11];
        memset(buffer, 0, 11);
        strncpy(buffer, hdr.size, 10);

        size_t data_size = strtoull(buffer, NULL, 10);

        struct bfld_archive_member *member = malloc(sizeof(struct bfld_archive_member) + data_size);
        if (member == NULL) {
            bfld_free_archive(&ar);
            return ENOMEM;
        }

        member->size = data_size;
        
        if (fread(member->data, data_size, 1, fp) <= 0) {
            free(member);
            bfld_free_archive(&ar);
            return EINVAL;
        }

        bfld_list_insert(&ar->members, member, list_entry);
    }

    *archive = ar;
    return 0;
}


void bfld_free_archive(struct bfld_archive **archive)
{
    struct bfld_archive *ar = *archive;

    if (ar != NULL) {
        bfld_list_foreach(struct bfld_archive_member, it, &ar->members, list_entry) {
            free(it);
        }

        free(ar);
        *archive = NULL;
    }
}

