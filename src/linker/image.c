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
#include "utils/list.h"
#include "utils/rbtree.h"


struct output_section * 
image_create_output_section(struct image *img, 
                            enum section_type type,
                            const char *name)
{
    assert(name != NULL);
    
    log_ctx_new(img->name);

    struct rb_node **pos = &(img->section_map.root), *parent = NULL;

    while (*pos != NULL) {
        struct output_section *this = rb_entry(*pos, struct output_section, map_entry);
        parent = *pos;

        int result = strcmp(name, this->name);
        if (result < 0) {
            pos = &((*pos)->left);
        } else if (result > 0) {
            pos = &((*pos)->right);
        } else {
            // An output section with the same name was already created
            log_error("Output section %s was already created", this->name);
            log_ctx_pop();
            return NULL;
        }
    }

    struct output_section *out = malloc(sizeof(struct output_section));
    if (out == NULL) {
        log_ctx_pop();
        return NULL;
    }

    out->name = strdup(name);
    if (out->name == NULL) {
        free(out);
        log_ctx_pop();
        return NULL;
    }

    out->image = img;
    out->type = type;
    list_insert_tail(&img->sections, &out->list_entry);
    rb_insert_node(&out->map_entry, parent, pos);
    rb_insert_fixup(&img->section_map, &out->map_entry);
    list_head_init(&out->links);

    out->file_offset = 0;
    out->file_size = 0;
    out->vaddr = 0;
    out->size = 0;
    out->align = type == SECTION_TEXT ? img->cpu_align : 0;

    log_debug("Created output section %s", out->name);
    log_ctx_pop();
    return out;    
}


static void image_remove_output_section(struct output_section *outsect)
{
    struct image *img = outsect->image;
    log_ctx_push(LOG_CTX(.file=img->name, .section=outsect->name));

    log_trace("Removing output section");
    image_clear_output_section(outsect);

    rb_remove(&img->section_map, &outsect->map_entry);
    list_remove(&outsect->list_entry);

    free(outsect->name);
    free(outsect);

    log_ctx_pop();
}


void image_clear_output_section(struct output_section *outsect)
{
    struct image *img = outsect->image;
    log_ctx_push(LOG_CTX(.file = img->name, .section = outsect->name));

    log_trace("Clearing all previously added sections");

    list_for_each_entry_safe(link, &outsect->links, struct section_link, list_entry) {
        list_remove(&link->list_entry);
        rb_remove(&img->link_map, &link->map_entry);
        log_debug("Releasing section");
        section_put(link->section);
        free(link);
    }

    log_ctx_pop();
}


bool image_add_section(struct output_section *out, struct section *sect)
{
    struct image *img = out->image;
    struct rb_node **pos = &(img->link_map.root), *parent = NULL;

    log_ctx_push(LOG_CTX(.file = img->name, .section = out->name));

    while (*pos != NULL) {
        struct section_link *this = rb_entry(*pos, struct section_link, map_entry);
        parent = *pos;

        if (sect < this->section) {
            pos = &((*pos)->left);
        } else if (sect > this->section) {
            pos = &((*pos)->right);
        } else {
            log_error("Input section was previously added to output section %s", 
                    this->output->name);
            log_ctx_pop();
            return false;
        }
    }

    struct section_link *link = malloc(sizeof(struct section_link));
    if (link == NULL) {
        return false;
    }

    link->output = out;
    list_insert_tail(&out->links, &link->list_entry);
    rb_insert_node(&link->map_entry, parent, pos);
    rb_insert_fixup(&img->link_map, &link->map_entry);
    link->section = section_get(sect);

    link->size = sect->size;
    link->align = sect->align;
    if (sect->align > out->align) {
        out->align = sect->align;
    }

    link->offset = align_to(out->size, link->align);
    out->size += link->size;

    log_trace("Added input section to output section %s", out->name);

    log_ctx_pop();
    return true;
}


struct image * image_alloc(const char *name, uint32_t march)
{
    log_ctx_new(name);

    const struct target *target = target_lookup(march);
    if (target == NULL) {
        log_fatal("Unsupported machine code architecture");
        log_ctx_pop();
        return NULL;
    }

    struct image *img = malloc(sizeof(struct image));
    if (img == NULL) {
        log_ctx_pop();
        return NULL;
    }

    img->name = NULL;
    if (name != NULL) {
        img->name = strdup(name);
    }

    // Some target information
    img->target = march;
    img->cpu_align = target->cpu_align;
    img->min_page_size = target->min_page_size;
    img->max_page_size = target->max_page_size;
    img->section_boundary = target->section_boundary;
    img->is_be = target->is_be;

    img->base_addr = 0;
    img->entry_addr = 0;
    img->size = 0;
    img->file_size = 0;

    img->refcnt = 1;
    
    memset(&img->symbols, 0, sizeof(struct symbols));
    list_head_init(&img->sections);
    rb_tree_init(&img->section_map);
    rb_tree_init(&img->link_map);

    log_debug("Created image");
    log_ctx_pop();
    return img;
}


void image_put(struct image *img)
{
    assert(img != NULL);
    assert(img->refcnt > 0);

    if (--(img->refcnt) == 0) {
        log_ctx_new(img->name);
        log_trace("Destroying image");

        symbols_clear(&img->symbols);

        list_for_each_entry_safe(outsect, &img->sections, struct output_section, list_entry) {
            image_remove_output_section(outsect);
        }

        assert(list_empty(&img->sections) && "Output sections is not empty");
        assert(rb_tree_empty(&img->section_map) && "Output section map is not empty");
        assert(rb_tree_empty(&img->link_map) && "Section link map is not empty");

        if (img->name != NULL) {
            free(img->name);
        }
        free(img);
        log_ctx_pop();
    }
}


struct image * image_get(struct image *img)
{
    assert(img != NULL);
    assert(img->refcnt > 0);
    img->refcnt++;
    return img;
}


static struct section_link * lookup_section_link(const struct image *img, const struct section *sect)
{
    struct rb_node *node = img->link_map.root;

    while (node != NULL) {
        struct section_link *this = rb_entry(node, struct section_link, map_entry);

        if (sect < this->section) {
            node = node->left;
        } else if (sect > this->section) {
            node = node->right;
        } else {
            return this;
        }
    }

    return NULL;
}


bool image_add_symbol(struct image *img, struct symbol *sym)
{
    log_ctx_new(img->name);

    if (sym->section != NULL) {
        if (lookup_section_link(img, sym->section) != NULL) {
            log_trace("Adding symbol '%s' to image", sym->name);
            log_ctx_pop();
            return symbols_push(&img->symbols, sym);
        }

        log_error("Image is mising symbol definition for symbol '%s'", sym->name);
        log_ctx_pop();
        return false;

    } else if (sym->is_absolute) {
        log_debug("Adding absolute symbol '%s' to image", sym->name);
        log_ctx_pop();
        return symbols_push(&img->symbols, sym);
    }

    log_error("Could not add symbol '%s' to image; symbol is undefined", sym->name);

    log_ctx_pop();
    return false;
}


struct output_section *
image_find_output_section(const struct image *img, const char *name)
{
    struct rb_node *node = img->section_map.root;

    while (node != NULL) {
        struct output_section *this = rb_entry(node, struct output_section, map_entry);

        int result = strcmp(name, this->name);
        if (result < 0) {
            node = node->left;
        } else if (result > 0) {
            node = node->right;
        } else {
            return this;
        }
    }

    return NULL;
}


struct output_section *
image_get_output_section(const struct image *img, const struct section *sect)
{
    const struct section_link *link = lookup_section_link(img, sect);
    
    if (link != NULL) {
        return link->output;
    }

    return NULL;
}


uint64_t image_get_section_address(const struct image *img, const struct section *sect)
{
    const struct section_link *link = lookup_section_link(img, sect);

    if (link == NULL) {
        return 0;
    }

    return link->vaddr;
}


uint64_t image_get_symbol_address(const struct image *img, const struct symbol *sym)
{
    if (!symbol_is_defined(sym)) {
        return 0;
    }

    if (sym->is_absolute) {
        return sym->offset;
    }

    const struct section_link *link = lookup_section_link(img, sym->section);
    if (link == NULL) {
        return 0;
    }

    return link->vaddr + sym->offset;
}


void image_layout(struct image *img, uint64_t base_addr)
{
    img->base_addr = base_addr;
    uint64_t vaddr = base_addr;

    list_for_each_entry_safe(outsect, &img->sections, struct output_section, list_entry) {
        if (outsect->size == 0) {
            image_remove_output_section(outsect);
            continue;
        }

        outsect->align = outsect->align < img->section_boundary ? img->section_boundary : outsect->align;
        vaddr = align_to(vaddr, outsect->align);
        outsect->vaddr = vaddr;
        uint64_t offset = 0;

        list_for_each_entry(link, &outsect->links, struct section_link, list_entry) {
            offset = align_to(offset, link->align);
            assert(align_to(offset, link->align) == offset);
            link->vaddr = align_to(vaddr, link->align);
            offset += link->size;
            vaddr += link->size;
        }

        assert(outsect->size == offset);
        assert(outsect->vaddr + offset == vaddr);
    }

    img->size = vaddr - base_addr;
}

//void image_pack(struct image *img, uint64_t base_addr)
//{
//    img->base_addr = base_addr;
//    uint64_t vaddr = base_addr;
//
//    list_for_each_entry_safe(grp, &img->groups, struct section_group, list_entry) {
//        if (grp->size == 0) {
//            remove_group(grp);
//        } else {
//            grp->align = grp->align < img->max_page_size ? img->max_page_size : grp->align;
//            grp->vaddr = align_to(vaddr, grp->align);
//            vaddr = align_to(grp->vaddr + grp->size, img->max_page_size);
//        }
//    }
//
//    img->size = vaddr - base_addr;
//}
