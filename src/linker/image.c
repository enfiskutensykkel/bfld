#include "logging.h"
#include "image.h"
#include "section.h"
#include "sections.h"
#include "symbols.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "utils/align.h"


static struct section_group * add_group(struct image *img, enum section_type type)
{
    struct section_group *grp = malloc(sizeof(struct section_group));
    if (grp == NULL) {
        return NULL;
    }

    grp->name = strdup(section_type_to_string(type));
    grp->image = img;
    grp->type = type;
    grp->vaddr = 0;
    grp->size = 0;
    grp->align = 0;
    memset(&grp->sections, 0, sizeof(grp->sections));

    if (list_empty(&img->groups) || type == SECTION_ZERO) {
        list_insert_tail(&img->groups, &grp->list_entry);
    } else {
        struct section_group *last = list_last_entry(&img->groups, struct section_group, list_entry);
        if (last->type == SECTION_ZERO) {
            list_insert_tail(&last->list_entry, &grp->list_entry);
        }
    }

    return grp;
}


static struct section_group * get_group(struct image *img, enum section_type type)
{
    // Try to look up the group
    list_for_each_entry(g, &img->groups, struct section_group, list_entry) {
        if (g->type == type) {
            return g;
        }
    }

    // Couldn't find group, try to create it
    log_trace("Creating section group '%s'", section_type_to_string(type));
    return add_group(img, type);
}


static void remove_group(struct section_group *grp)
{
    sections_clear(&grp->sections);
    list_remove(&grp->list_entry);
    if (grp->name != NULL) {
        free(grp->name);
    }
    free(grp);
}


struct image * image_alloc(const char *name, uint32_t target, uint64_t cpu_align,
                           uint64_t min_page_size, uint64_t max_page_size, bool is_be)
{
    struct image *img = malloc(sizeof(struct image));
    if (img == NULL) {
        return NULL;
    }

    img->name = NULL;

    if (name != NULL) {
        img->name = strdup(name);
    }

    img->base_addr = 0;
    img->size = 0;
    img->refcnt = 1;
    img->entrypoint = 0;
    img->target = target;
    img->cpu_align = cpu_align;
    img->min_page_size = min_page_size;
    img->max_page_size = max_page_size;
    img->is_be = is_be;

    list_head_init(&img->groups);
    memset(&img->symbols, 0, sizeof(img->symbols));
    return img;
}


void image_put(struct image *img)
{
    assert(img != NULL);
    assert(img->refcnt > 0);

    if (--(img->refcnt) == 0) {
        symbols_clear(&img->symbols);

        list_for_each_entry_safe(grp, &img->groups, struct section_group, list_entry) {
            remove_group(grp);
        }

        if (img->name != NULL) {
            free(img->name);
        }
        free(img);
    }
}


struct image * image_get(struct image *img)
{
    assert(img != NULL);
    assert(img->refcnt > 0);
    img->refcnt++;
    return img;
}


bool image_add_section(struct image *img, struct section *sect)
{
    struct section_group *grp = get_group(img, sect->type);
    if (grp == NULL) {
        return false;
    }

    if (sections_push(&grp->sections, sect) > 0) {
        if (sect->type == SECTION_TEXT && sect->align <= img->cpu_align) {
            sect->align = img->cpu_align;
        }

        if (sect->align > grp->align) {
            grp->align = sect->align;
        }

        return true;
    }

    return false;
}


bool image_reserve_capacity(struct image *img, enum section_type type, size_t n)
{
    struct section_group *grp = get_group(img, type);
    if (grp == NULL) {
        return false;
    }

    return sections_reserve(&grp->sections, n);
}


void image_pack(struct image *img, uint64_t base_addr)
{
    img->base_addr = base_addr;
    uint64_t vaddr = base_addr;

    list_for_each_entry(grp, &img->groups, struct section_group, list_entry) {
        grp->vaddr = align_to(vaddr, grp->align);
        uint64_t offset = 0;

        for (size_t i = 1; i <= grp->sections.maxidx; ++i) {
            struct section *sect = sections_at(&grp->sections, i);
            sect->vaddr = align_to(grp->vaddr + offset, sect->align);
            offset = (sect->vaddr - grp->vaddr) + sect->size;
        }

        grp->size = offset;
        vaddr = align_to(grp->vaddr + grp->size, img->max_page_size);
    }

    img->size = vaddr - base_addr;
}
