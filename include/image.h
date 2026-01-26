#ifndef BFLD_IMAGE_H
#define BFLD_IMAGE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "utils/rbtree.h"
#include "utils/list.h"
#include "sectiontype.h"
#include "symbols.h"


// Forward declarations
struct section;
struct symbol;


/*
 * The linked output image.
 */
struct image
{
    char *name;
    uint32_t target;                // target architecture
    uint64_t cpu_align;             // CPU code alignment requirements
    uint64_t min_page_size;         // minimum page size
    uint64_t max_page_size;         // maximum page size
    uint64_t section_boundary;      // minimum boundary/alignment between sections with different attributes
    bool is_be;                     // is big-endian?
    uint64_t base_addr;             // base virtual address of the image
    uint64_t entry_addr;            // address of the image's entrypoint
    uint64_t size;                  // total memory size
    size_t file_size;               // total file size
    int refcnt;                     // reference counter
    struct symbols symbols;         // symbols used by the image (used for writing symbol table to file)
    struct list_head segments;      // list of program segments
    struct rb_tree order_map;       // map of output sections by order
    struct rb_tree name_map;        // map of output sections by name
    struct rb_tree link_map;        // map of section->output_section links by input section pointer address
};


/*
 * Segment attributes
 */
#define SEGMENT_READABLE    (1 << 0)
#define SEGMENT_WRITABLE    (1 << 1)
#define SEGMENT_EXECUTABLE  (1 << 2)


/*
 * Loadable program segment.
 * Whereas sections contain content, segments describe what should 
 * actually be loaded into memory and how.
 */
struct segment
{
    struct image *image;            // weak reference to the output image
    struct list_head list_entry;    // linked list entry
    uint32_t attrs;                 // segment attributes
    char *name;                     // name of the segment
    uint64_t vaddr;                 // base virtual address for the segment
    uint64_t size;                  // memory size of the segment
    size_t file_offset;             // offset in the output file
    size_t file_size;               // size in the output file
    struct list_head sections;      // list of output sections
};


/*
 * Output section.
 *
 * This refers to a group input sections that should be
 * grouped together in the final output image as a single
 * section.
 */
struct output_section
{
    struct image *image;            // weak reference to the output image
    char *name;                     // section name
    enum section_type type;         // type of this section group
    struct rb_node order_map_entry; // order map entry on image
    struct list_head list_entry;    // linked list entry on segment
    struct rb_node name_map_entry;  // name map entry on image
    size_t file_offset;             // offset in the output file
    size_t file_size;               // size in the output file
    uint64_t vaddr;                 // base virtual address for sections in the group
    uint64_t size;                  // total memory size of the section
    uint64_t align;                 // memory alignment requirements
    struct list_head links;         // list of input sections that go into this section
    bool metadata;                  // section is metadata (debug symbols, symtab, etc)
};


/*
 * Link between an input section and the output section.
 */
struct section_link
{
    struct output_section *output;  // weak reference to the output section
    struct section *section;        // strong reference to the input section
    struct rb_node map_entry;       // map entry in the section address map
    struct list_head list_entry;    // linked list entry in output section
    uint64_t align;                 // memory aligntment requirements
    uint64_t vaddr;                 // finalized virtual memory address
    uint64_t size;                  // total memory size
    uint64_t offset;                // offset from the base of the output section
};


static inline 
bool segment_is_readable(const struct segment *seg)
{
    return !!(seg->attrs & SEGMENT_READABLE);
}


static inline
bool segment_is_writable(const struct segment *seg)
{
    return !!(seg->attrs & SEGMENT_WRITABLE);
}


static inline
bool segment_is_executable(const struct segment *seg)
{
    return !!(seg->attrs & SEGMENT_EXECUTABLE);
}


/*
 * Helper function to sort sections based on their type.
 */
static inline
uint32_t section_type_to_order(enum section_type type)
{
    switch (type) {
        case SECTION_TEXT:
            return 0;
        case SECTION_RODATA:
            return 1;
        case SECTION_DATA:
            return 2;
        case SECTION_ZERO:
        default:
            return UINT32_MAX;
    }
}


static inline
uint32_t section_type_to_segment_attrs(enum section_type)
{
    switch (type) {
        case SECTION_TEXT:
            return SEGMENT_READABLE | SEGMENT_EXECUTABLE;
        case SECTION_RODATA:
            return SEGMENT_READABLE;
        case SECTION_DATA:
            return SEGMENT_READABLE | SEGMENT_WRITABLE;
        case SECTION_DATA:
            return SEGMENT_READABLE | SEGMENT_WRITABLE;
        default:
            return 0;
    }
}


/*
 * Create an output image.
 */
struct image * image_alloc(const char *name, uint32_t target);


/*
 * Take an image reference.
 */
struct image * image_get(struct image *image);


/*
 * Release image reference.
 */
void image_put(struct image *image);


/*
 * Add a symbol to the image's symbol table.
 *
 * This has no meaning for the image's address layout, but
 * is needed for adding symbols to the final symbol table that
 * is written to file.
 *
 * The symbol's section must have been added to the image first.
 *
 * This takes a symbol reference on successful insertion.
 */
bool image_add_symbol(struct image *image, struct symbol *symbol);


/*
 * Create an output section of the given type with a given name.
 * The name must be unique.
 */
struct output_section * 
image_create_output_section(struct image *image, enum section_type type,
                            const char *name);


/*
 * Look up an output section by its name.
 */
struct output_section * 
image_find_output_section(const struct image *image, const char *name);


/*
 * Look up an output section from an input section.
 */
struct output_section * 
image_get_output_section(const struct image *image, const struct section *section);


/*
 * Look up the finalized virtual address of a section.
 */
uint64_t image_get_section_address(const struct image *image, 
                                   const struct section *section);


/*
 * Look up the finalized virtual address for a symbol definition.
 */
uint64_t image_get_symbol_address(const struct image *image, 
                                  const struct symbol *symbol);


/*
 * Link an input section to an output section.
 * The section must not have been added to the image already.
 * This takes a section reference on successful insertion.
 */
bool image_add_section(struct output_section *output, struct section *section);


/*
 * Remove all linked sections from the output section.
 * Releases all section references.
 */
void image_clear_output_section(struct output_section *output);


/*
 * Finalize the memory layout of the image.
 *
 * Calculates output section's file offsets, virtual addresses 
 * and alignment based on the given base address.
 */
void image_layout(struct image *image, uint64_t base_address);


/*
 * Partition output sections into program segments.
 */
void image_partition_segments(struct image *image);


#ifdef __cplusplus
}
#endif
#endif
