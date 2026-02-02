#ifndef BFLD_LINKER_H
#define BFLD_LINKER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "utils/list.h"
#include "sections.h"
#include "symbols.h"
#include "globals.h"
#include "archive_index.h"

/* Some forward declarations */
struct objectfile;
struct objectfile_reader;
struct archive;
struct archive_reader;


/* 
 * Linker context.
 */
struct linkerctx
{
    char *name;                     // output file name
    int refcnt;                     // reference counter
    bool gc_sections;               // should we keep all sections and symbols?
    struct archive_index archives;  // archive index
    struct globals globals;         // global symbols
    struct sections sections;       // worklist of input sections
    struct symbols unresolved;      // queue of unresolved symbols

    uint32_t target_march;          // target machine code architecture
    uint64_t target_cpu_align;      // minimum CPU code alignment requirement
    uint64_t target_pgsz_min;       // target minimum page size
    uint64_t target_pgsz_max;       // target maximum page size
    uint64_t target_sect_align;     // minimum boundary/alignment between sections with different attributes
    bool target_is_be;              // is target big endian?
    
    uint64_t base_addr;             // base virtual address of the image
    uint64_t entry_addr;            // address of the image's entrypoint
};


/*
 * Create linker context.
 */
struct linkerctx * linker_alloc(const char *name, uint32_t target);


/*
 * Take a linker context reference.
 */
struct linkerctx * linker_get(struct linkerctx *ctx);


/*
 * Release a linker context reference.
 */
void linker_put(struct linkerctx *ctx);


bool linker_load_objectfile(struct linkerctx *ctx,
                            struct objectfile *objectfile,
                            const struct objectfile_reader *frontend);



bool linker_read_archive(struct linkerctx *ctx,
                         struct archive *archive,
                         const struct archive_reader *reader);


/*
 * Resolve all undefined global symbols and add them
 * to the linker's globals.
 */
bool linker_resolve_globals(struct linkerctx *ctx);


/*
 * Mark sections and symbols as alive.
 */
void linker_gc_sections(struct linkerctx *ctx);


/*
 * Create a common section.
 */
bool linker_create_common_section(struct linkerctx *ctx);


#ifdef __cplusplus
}
#endif
#endif
