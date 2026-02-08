#include "logging.h"
#include "groups.h"
#include "section.h"
#include "sections.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "utils/hash.h"


static void remove_group(struct groups *groups, size_t slot)
{
    if (slot >= groups->capacity) {
        return;
    }

    struct group *this = &groups->table[slot];
    struct group *next = &groups->table[(slot + 1) & (groups->capacity - 1)];
    while (next->hash != 0 && next->dfi != 0) {
        *this = *next;
        this->dfi--;
        this = next;
    }

    this->hash = 0;
    this->dfi = 0;
    free(this->name);
    this->name = NULL;

    struct section *sect;
    while ((sect = sections_pop(&this->sections)) != NULL) {
        sect->group = NULL;
        section_put(sect);
    }

    sections_clear(&this->sections);
    groups->ngroups--;
}


size_t find_group_slot(const struct groups *groups, const char *name)
{
    if (groups->ngroups == 0) {
        return groups->capacity;
    }

    uint32_t hash = hash_fnv1a_32(name, strlen(name));
    if (hash == 0) {
        hash = 1;
    }

    size_t slot = hash & (groups->capacity - 1);
    struct group *this = &groups->table[slot];
    size_t dfi = 0;

    while (this->hash != 0 && dfi <= this->dfi) {
        if (this->hash == hash) {
            if (strcmp(this->name, name) == 0) {
                return slot;
            }
        }

        slot = (slot + 1) & (groups->capacity - 1);
        this = &groups->table[slot];
        ++dfi;
    }

    return groups->capacity;
}


static bool rehash(struct groups *groups)
{
    size_t capacity = groups->capacity > 0 ? groups->capacity * 2 : 32;
    if (capacity * sizeof(struct group) < groups->capacity * sizeof(struct group)) {
        return false;
    }

    struct group *table = calloc(capacity, sizeof(struct group));
    if (table == NULL) {
        return false;
    }

    for (size_t i = 0; i < groups->capacity; ++i) {
        struct group current = groups->table[i];

        if (current.hash == 0) {
            continue;
        }

        current.dfi = 0;
        size_t slot = current.hash & (capacity - 1);

        while (current.hash != 0) {
            struct group *this = &table[slot];

            if (this->hash == 0 || current.dfi > this->dfi) {
                struct group tmp = *this;
                *this = current;
                current = tmp;
            }

            slot = (slot + 1) & (capacity - 1);
            current.dfi++;
        }
    }

    free(groups->table);
    groups->table = table;
    groups->capacity = capacity;
    groups->rehash_threshold = (capacity / 4) * 3;
    return true;
}


void groups_clear(struct groups *groups)
{
    for (size_t i = 0; groups->ngroups > 0 && i < groups->capacity; ++i) {
        struct group *this = &groups->table[i];

        if (this->hash != 0) {
            struct section *sect;

            while ((sect = sections_pop(&this->sections)) != NULL) {
                sect->group = NULL;
                section_put(sect);
            }
            sections_clear(&this->sections);
            free(this->name);

            groups->ngroups--;
        }
    }
    free(groups->table);
    groups->table = NULL;
    groups->capacity = 0;
    groups->ngroups = 0;
    groups->rehash_threshold = 0;
}


struct group * groups_lookup(const struct groups *groups, const char *name)
{
    size_t slot = find_group_slot(groups, name);
    if (slot < groups->capacity) {
        struct group *group = &groups->table[slot];
        return group;
    }
    return NULL;
}


void groups_remove(struct groups *groups, const char *name)
{
    size_t slot = find_group_slot(groups, name);
    if (slot < groups->capacity) {
        remove_group(groups, slot);
    }
}


struct group * groups_create(struct groups *groups, const char *name, size_t nsections)
{
    size_t slot = find_group_slot(groups, name);
    if (slot < groups->capacity) {
        struct group *group = &groups->table[slot];
        if (group->sections.q.capacity < nsections) {
            sections_reserve(&group->sections, nsections);
        }
        return group;
    }

    if (groups->ngroups >= groups->rehash_threshold) {
        if (!rehash(groups)) {
            return NULL;
        }
    }

    size_t length = strlen(name);
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    strcpy(copy, name);

    uint32_t hash = hash_fnv1a_32(name, length);
    if (hash == 0) {
        hash = 1;
    }

    struct group current = (struct group) {
        .hash = hash,
        .dfi = 0,
        .name = copy,
        .sections = {0}
    };

    slot = current.hash & (groups->capacity - 1);
    size_t inserted = groups->capacity;

    while (current.hash != 0) {
        struct group *this = &groups->table[slot];

        if (this->hash == 0 || current.dfi > this->dfi) {
            struct group tmp = *this;
            *this = current;
            current = tmp;
            if (inserted == groups->capacity) {
                groups->ngroups++;
                inserted = slot;
            }
        }

        slot = (slot + 1) & (groups->capacity - 1);
        current.dfi++;
    }

    return &groups->table[inserted];
}
