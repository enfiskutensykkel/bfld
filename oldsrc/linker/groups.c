#include "logging.h"
#include "groups.h"
#include "strpool.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


void groups_clear(struct groups *groups)
{
    strpool_clear(&groups->signatures);
    free(groups->comdat);
    groups->comdat = NULL;
}


static bool extend_bitmap(struct groups *groups, uint64_t oldsize)
{
    uint64_t elems = (oldsize + 63) / 64;
    uint64_t nelems = (groups->signatures.size + 63) / 64;

    if (nelems <= elems) {
        return true;
    }

    uint64_t *bitmap = realloc(groups->comdat, sizeof(uint64_t) * nelems);
    if (bitmap == NULL) {
        return false;
    }

    memset(&bitmap[elems], 0, (nelems - elems) * sizeof(uint64_t));
    groups->comdat = bitmap;
    return true;
}


uint64_t groups_create_group(struct groups *groups,
                             const char *signature,
                             bool comdat)
{
    uint64_t size = groups->signatures.size;

    uint64_t group_id = strpool_intern(&groups->signatures, signature);
    if (group_id == 0) {
        return 0;
    }

    if (groups->comdat == NULL || size != groups->signatures.size) {
        if (!extend_bitmap(groups, groups->comdat != NULL ? size : 0)) {
            return 0;
        }
    }

    if (comdat) {
        groups->comdat[group_id >> 6] |= 1ULL << (group_id & 63);
    }

    return group_id;
}
