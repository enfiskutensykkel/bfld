#ifndef _BFLD_BACKEND_H
#define _BFLD_BACKEND_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "utils/bswap.h"


/*
 * Maximum number of relocation types.
 */
#define BACKEND_MAX_RELOC_TYPES   256


/*
 * Architecture specific binary file back-end operations.
 */
struct backend
{
    /*
     * Name of the back-end.
     */
    const char *name;

    /*
     * Machine code architecture the back-end supports.
     */
    uint32_t march;

    /*
     * Machine code architecture alignment.
     */
    uint64_t march_align;

    /*
     * Size of an entry in the Global Offset Table (GOT).
     */
    size_t got_entry_size;

    /*
     * Write an address entry into the GOT.
     *
     * If this function returns anything but 0, it is assumed
     * to mean that a fatal error occurred and all further 
     * processing is aborted.
     */
    int (*write_got_entry)(uint8_t *entryptr, uint64_t vaddr);

    /*
     * Size of an entry in the Procedure Linkage Table (PLT)
     */
    size_t plt_entry_size;

    /*
     * Size of the PLT header.
     */
    size_t plt_header_size;

    /*
     *
     */
    int (*write_plt_header)(uint8_t *plt_content,      // where to write the header
                            uint64_t vaddr,            // address of the header
                            uint64_t got_plt_vaddr);   // address of the GOT/PLT

    /*
     * Write PLT trampoline stub.
     *
     * If this function returns anything but 0, it is assumed
     * to mean that a fatal error occurred and all further 
     * processing is aborted.
     */
    int (*write_plt_stub)(uint8_t *plt_content,         // pointer to where to write the PLT stub
                          uint64_t plt_vaddr,           // address of the PLT stub
                          uint64_t got_entry_vaddr);

    /*
     * Apply relocation to section content.
     *
     * If this function returns anything but 0, it is assumed
     * to mean that a fatal error occurred and all further 
     * processing is aborted.
     */
    int (*apply_reloc)(uint8_t *content,        // pointer to the start of the section content
                       uint64_t offset,         // offset to where to apply the relocation
                       uint64_t baseaddr,       // absolute base address of the section
                       uint64_t targetaddr,     // absolute address of the target
                       int64_t addend,          // relocation addend
                       uint32_t reloc_type);    // relocation type

};


/*
 * Register a linker back-end.
 */
void backend_register(const struct backend *backend);


/*
 * Look up a linker back-end based on the machine code architecture.
 */
const struct backend * backend_lookup(uint32_t march);


#ifdef __cplusplus
}
#endif
#endif
