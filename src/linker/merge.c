#include "merge.h"
#include "objfile.h"
#include "secttypes.h"
#include "utils/list.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "logging.h"
#include "align.h"
#include <assert.h>


int merged_init(struct merged_section **merged, const char *name, 
                enum section_type type)
{
    *merged = NULL;
    struct merged_section *m = malloc(sizeof(struct merged_section));
    if (m == NULL) {
        return ENOMEM;
    }

    if (name != NULL) {
        m->name = strdup(name);
        if (m->name == NULL) {
            free(m);
            return ENOMEM;
        }
    }

    m->refcnt = 1;
    m->type = type;
    list_head_init(&m->mappings);
    m->addr = 0;
    m->align = 1;
    m->total_size = 0;

    *merged = m;
    return 0;
}


void merged_get(struct merged_section *merged)
{
    ++(merged->refcnt);
}


void merged_put(struct merged_section *merged)
{
    if (--(merged->refcnt) == 0) {
        
        list_for_each_entry_safe(map, &merged->mappings, struct section_mapping, list_node) {
            assert(map->merged_section == merged);

            list_remove(&map->list_node);
            objfile_put(map->objfile);
            free(map);
        }

        if (merged->name != NULL) {
            free(merged->name);
        }
        free(merged);
    }
}


int merged_add_section(struct merged_section *merged, struct section *sect)
{
    if (sect->type != merged->type) {
        return EINVAL;
    }

    struct section_mapping *map = malloc(sizeof(struct section_mapping));
    if (map == NULL) {
        return ENOMEM;
    }

    map->merged_section = merged;
    map->objfile = sect->objfile;
    objfile_get(map->objfile);
    map->section = sect;
    map->offset = 0;
    map->content = sect->content;
    map->size = sect->size;

    if (sect->align > merged->align) {
        merged->align = sect->align;
    }

    list_insert_tail(&merged->mappings, &map->list_node);

    return 0;
}


int merged_calculate_offsets(struct merged_section *merged, uint64_t base_addr)
{
    if (base_addr != BFLD_ALIGN(base_addr, merged->align)) {
        log_error("Base address 0x%lx is not aligned to 0x%lx",
                base_addr, merged->align);
        return EINVAL;
    }

    merged->addr = base_addr;

    uint64_t offset = 0;
    list_for_each_entry(m, &merged->mappings, struct section_mapping, list_node) {
        m->offset = offset;
        offset += merged->align > 1 ? BFLD_ALIGN(m->size, merged->align) : m->size;
    }

    merged->total_size = offset;
    return 0;
}


int merged_load_contents(struct merged_section *merged)
{
    if (merged->content != NULL) {
        return EEXIST;
    }

    if (merged->total_size == 0 || list_empty(&merged->mappings)) {
        return EINVAL;
    }

    return 0;
}

