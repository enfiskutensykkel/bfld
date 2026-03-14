#include "archive.h"
#include "logging.h"
#include "objectfile.h"
#include "mfile.h"
#include "strpool.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <utils/list.h>


struct objectfile * archive_extract_member(struct archive_member *member)
{
    if (member->objfile == NULL) {
        struct archive *ar = member->archive;
        char *name = NULL;
        const char *member_name = archive_member_name(member);

        if (member_name[0] != '\0') {
            name = malloc(strlen(ar->name) + 2 + strlen(member_name) + 1);
            if (name != NULL) {
                sprintf(name, "%s(%s)", ar->name, member_name);
            }
        } else {
            name = malloc(strlen(ar->name) + 24);
            if (name != NULL) {
                sprintf(name, "%s:%lu", ar->name, member->offset);
            }
        }

        if (name == NULL) {
            return NULL;
        }

        member->objfile = objectfile_alloc(ar->file, name, member->content, member->size);
        if (member->objfile == NULL) {
            free(name);
            return NULL;
        }
        free(name);
    }

    return objectfile_get(member->objfile);
}


struct archive_member * archive_get_member(const struct archive *ar, size_t offset)
{
    if (ar->nmembers > 0) {
        size_t low = 0;
        size_t high = ar->nmembers - 1;

        while (low <= high) {
            size_t mid = low + (high - low) / 2;
            struct archive_member *this = &ar->members[mid];

            if (this->offset == offset) {
                return this;
            } else if (this->offset < offset) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
    }

    return NULL;
}


struct archive_member * archive_add_member(struct archive *ar, 
                                           const char *name,
                                           size_t offset,
                                           size_t size)
{
    if (offset + size > ar->file_size) {
        log_error("Invalid offset and size for archive member");
        return NULL;
    }

    size_t low = 0;

    if (ar->members != NULL) {
        size_t high = ar->nmembers;

        while (low < high) {
            size_t mid = low + ((high - low) >> 1);

            if (ar->members[mid].offset == offset) {
                log_error("Member with offset %zu is already added to archive", offset);
                return NULL;
            } else if (ar->members[mid].offset < offset) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
    }

    struct archive_member *members = (struct archive_member*) realloc(ar->members, sizeof(struct archive_member) * (ar->nmembers + 1));
    if (members == NULL) {
        return NULL;
    }

    if (low < ar->nmembers) {
        memmove(&members[low + 1],
                &members[low],
                (ar->nmembers - low) * sizeof(struct archive_member));
    }

    struct archive_member *member = &members[low];
    member->name = strpool_intern(&ar->names, name);
    member->archive = ar;
    member->offset = offset;
    member->size = size;
    member->content = ar->file_data + offset;
    member->objfile = NULL;

    ar->members = members;
    ar->nmembers++;
    return member;
}


void archive_put(struct archive *ar)
{
    assert(ar != NULL);
    assert(ar->refcnt > 0);

    if (--(ar->refcnt) == 0) {

        if (ar->members != NULL) {
            for (size_t i = 0; i < ar->nmembers; ++i) {
                struct archive_member *member = &ar->members[i];
                if (member->objfile != NULL) {
                    objectfile_put(member->objfile);
                }
            }
            free(ar->members);
        }
        ar->members = NULL;
        ar->nmembers = 0;
        
        mfile_put(ar->file);
        strpool_clear(&ar->names);
        free(ar->name);
        free(ar);
    }
}


struct archive * archive_get(struct archive *ar)
{
    assert(ar != NULL);
    assert(ar->refcnt > 0);
    ++(ar->refcnt);
    return ar;
}


struct archive * archive_alloc(struct mfile *file,
                               const char *name,
                               const uint8_t *file_data,
                               size_t file_size)
{
    if (file_data == NULL) {
        file_data = (const uint8_t*) file->data;
        file_size = file->size;
    }

    if (file_data < ((const uint8_t*) file->data) || 
            (file_data + file_size) > (((const uint8_t*) file->data) + file->size)) {
        log_fatal("Archive file data content is outside valid range");
        return NULL;
    }

    if (name == NULL) {
        log_warning("Archive file has unknown name");
        name = "UNKNOWN";
    }

    struct archive *ar = malloc(sizeof(struct archive));
    if (ar == NULL) {
        return NULL;
    }

    ar->name = malloc(strlen(name) + 1);
    if (ar->name == NULL) {
        free(ar);
        return NULL;
    }
    strcpy(ar->name, name);

    memset(&ar->names, 0, sizeof(struct strpool));
    ar->file = mfile_get(file);
    ar->refcnt = 1;
    ar->file_data = file_data;
    ar->file_size = file_size;
    ar->members = NULL;
    ar->nmembers = 0;
    return ar;
}
