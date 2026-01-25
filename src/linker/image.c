#include "logging.h"
#include "image.h"
#include "section.h"
#include "symbols.h"
#include "symbol.h"
#include "target.h"
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
    grp->align = grp->type == SECTION_TEXT ? img->cpu_align : 0;
    list_head_init(&grp->sections);
    list_insert_tail(&img->groups, &grp->list_entry);
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

    return NULL;
}


static void remove_group(struct section_group *grp)
{
    list_for_each_entry_safe(outsect, &grp->sections, struct output_section, list_entry) {
        list_remove(&outsect->list_entry);
        section_put(outsect->section);
        free(outsect);
    }

    free(grp->name);
    list_remove(&grp->list_entry);
    free(grp);
}


struct image * image_alloc(const char *name, uint32_t march)
{
    const struct target *target = target_lookup(march);
    if (target == NULL) {
        log_fatal("Unsupported machine code architecture");
        return NULL;
    }

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
    img->entry = 0;
    img->target = target->march;
    img->cpu_align = target->cpu_align;
    img->min_page_size = target->min_page_size;
    img->max_page_size = target->max_page_size;
    img->is_be = target->is_be;

    memset(&img->symbols, 0, sizeof(struct symbols));
    list_head_init(&img->groups);

    // Create the different section groups
    add_group(img, SECTION_TEXT);
    add_group(img, SECTION_RODATA);
    add_group(img, SECTION_DATA);
    add_group(img, SECTION_ZERO);

    return img;
}


void image_put(struct image *img)
{
    assert(img != NULL);
    assert(img->refcnt > 0);

    if (--(img->refcnt) == 0) {

        list_for_each_entry_safe(grp, &img->groups, struct section_group, list_entry) {
            remove_group(grp);
        }

        symbols_clear(&img->symbols);

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
    assert(sect->output == NULL && "Section already assigned to an image");

    if (sect->output != NULL) {
        return false;
    }

    struct output_section *outsect = malloc(sizeof(struct output_section));
    if (outsect == NULL) {
        return false;
    }

    struct section_group *grp = get_group(img, sect->type);
    if (grp == NULL) {
        free(outsect);
        return false;
    }

    outsect->group = grp;
    outsect->section = section_get(sect);
    outsect->size = sect->size;
    sect->output = outsect;

    struct output_section *prev = list_last_entry(&grp->sections, 
                                                  struct output_section,
                                                  list_entry);

    list_insert_tail(&grp->sections, &outsect->list_entry);

    if (sect->align > grp->align) {
        grp->align = sect->align;
    }

    if (prev != NULL) {
        uint64_t offset = prev->offset + prev->size;
        outsect->offset = align_to(offset, sect->align);
    } else {
        outsect->offset = 0;
    }

    grp->size += outsect->size;
    log_debug("Added section %s to output section group %s", 
            sect->name, grp->name);
    return true;
}


bool image_add_symbol(struct image *img, struct symbol *sym)
{
    return symbols_push(&img->symbols, sym);
}


void image_pack(struct image *img, uint64_t base_addr)
{
    img->base_addr = base_addr;
    uint64_t vaddr = base_addr;

    list_for_each_entry_safe(grp, &img->groups, struct section_group, list_entry) {
        if (grp->size == 0) {
            remove_group(grp);
        } else {
            grp->align = grp->align < img->max_page_size ? img->max_page_size : grp->align;
            grp->vaddr = align_to(vaddr, grp->align);
            vaddr = align_to(grp->vaddr + grp->size, img->max_page_size);
        }
    }

    img->size = vaddr - base_addr;
}
