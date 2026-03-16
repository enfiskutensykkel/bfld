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
#include "groups.h"

/* Some forward declarations */
struct objectfile;
struct objectfile_reader;
struct archive;
struct archive_reader;
struct strpool;


/* 
 * Linker context.
 */
struct linkerctx
{
    const char *name;               // output file name
    int refcnt;                     // reference counter
    struct strpool *strings;        // global string table
    struct archives archives;       // archive symbol index
    struct globals globals;         // global symbols
    struct sections sections;       // worklist of input sections
    struct symbols unresolved;      // queue of unresolved symbols
    struct groups groups;           // section groups

    uint32_t target_march;          // target machine code architecture
    uint64_t target_ptr_size;       // pointer alignment for target machine code
    uint64_t target_cpu_align;      // minimum CPU code alignment requirement
    uint64_t target_got_entry_size; 
    uint64_t target_pgsz_min;       // target minimum page size
    uint64_t target_pgsz_max;       // target maximum page size
    uint64_t target_sect_align;     // minimum boundary/alignment between sections with different attributes
    bool target_is_be;              // is target big endian?

    uint64_t base_addr;             // base virtual address of the image
    uint64_t entry_addr;            // address of the image's entrypoint

    struct section *got;
    struct section *preinit_array;
    struct section *init_array;
    struct section *fini_array;
    struct section *init;
    struct section *fini;
};


/*
 * Helper function to look up a global symbol.
 */
static inline
struct symbol * linker_find_symbol(struct linkerctx *ctx, const char *name)
{
    return globals_find_symbol(&ctx->globals, name);
}


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





bool linker_add_marker_symbol(struct linkerctx *ctx,
                              const char *symbol_name,
                              struct section *section,
                              enum symbol_type symbol_type);


/*
 * Create synthetic section for the global offset table (GOT)
 */
bool linker_add_got_section(struct linkerctx *ctx);


/*
 * Add C runtime marker symbols.
 */
bool linker_add_crt_markers(struct linkerctx *ctx);


/*
 * Resolve all undefined global symbols and add them
 * to the linker's globals.
 */
bool linker_resolve_globals(struct linkerctx *ctx);


/*
 * Mark reachable sections and symbols as alive.
 * Part of dead code elimination (DCE).
 */
void linker_dce_mark(struct linkerctx *ctx, const struct symbols *keep);


/*
 * Remove sections and symbols that aren't marked as alive.
 * Part of dead code elimination (DCE)
 */
void linker_dce_sweep(struct linkerctx *ctx);


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
