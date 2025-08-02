#ifndef __BFLD_ARCHITECTURE_H__
#define __BFLD_ARCHITECTURE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "archtypes.h"
#include <stddef.h>
#include <stdint.h>


#define ARCH_HANDLER_MAX_RELOC_TYPES    256


struct arch_relocation
{
    uint32_t type;          // relocation type
    uint64_t addr;          // symbol address (finalized)
    uint64_t offset;        // offset within section
    int64_t addend;         // relocation addend (if applicable)
};


/*
 * Architecture-specific handling.
 *
 * The arch_handler is responsible for emitting architecture-specific
 * bytecode, such as relocations and GOT/PLT entries.
 */
struct arch_handler
{
    const char *name;

    enum arch_type arch_type;

    void (*create_plt_entry)(uint8_t *plt_content, uint64_t addr);

    size_t plt_entry_size;

    void (*setup_got_entry)(uint8_t *got_content, uint64_t addr);

    size_t got_entry_size;

    void (*apply_relocation)(const struct arch_relocation *reloc, 
                             uint64_t base_addr,
                             uint8_t *content);
};


/*
 * Look up the architecture handler for the specified architecture.
 */
const struct arch_handler * arch_handler_find(enum arch_type arch);


/*
 * Register an architecture handler.
 */
int arch_handler_register(const struct arch_handler *handler);


/*
 * Mark a an initializer so that it is invoked on startup.
 *
 * Example usage:
 *
 * const struct arch_handler my_handler = {
 *   .name = "my_x86-64_handler",
 *   .arch_type = ARCH_x86_64,
 *   .create_plt_entry = create_x86_64_plt,
 *   .plt_entry_size = 16,
 *   ...
 * }
 *
 * ARCH_HANDLER_INIT void my_handler_init(void) {
 *     ...
 *     arch_handler_register(&my_handler);
 * }
 */
#define ARCH_HANDLER_INIT __attribute__((constructor))


#ifdef __cplusplus
}
#endif
#endif
