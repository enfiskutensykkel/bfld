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
#include "archives.h"

/* Some forward declarations */
struct objectfile;
struct objectfile_reader;
struct archive;
struct archive_reader;
struct layout;


/* 
 * Linker context.
 */
struct linkerctx
{
    char *name;                     // output file name
    int refcnt;                     // reference counter
    bool gc_sections;               // should we keep all sections and symbols?
    struct archives archives;       // archive symbol index
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


/*
 * Add an input object file to be linked.
 */
bool linker_load_objectfile(struct linkerctx *ctx,
                            struct objectfile *objectfile,
                            const struct objectfile_reader *frontend);

/*
 * Read archive and add symbols it provide to the archive symbol index.
 * This allows the linker to lazily load object files from the archive
 * in order to resolve symbols.
 */
bool linker_read_archive(struct linkerctx *ctx,
                         struct archive *archive,
                         const struct archive_reader *reader);


bool linker_create_synthetic_sections(struct linkerctx *ctx);


/*
 * Resolve all undefined global symbols and add them
 * to the linker's globals.
 */
bool linker_resolve_globals(struct linkerctx *ctx);


/*
 * Mark reachable sections and symbols as alive, and
 * sweep those that aren't.
 *
 * This is effectively a dead-code elimination (DCE).
 */
void linker_gc_sections(struct linkerctx *ctx);


/*
 * Create a common section.
 *
 * This should ideally be done after DCE, since it allows us to prune
 * symbols that aren't referenced by relocations.
 */
bool linker_create_common_section(struct linkerctx *ctx);


#ifdef __cplusplus
}
#endif
#endif
