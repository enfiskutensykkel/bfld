#ifndef __BFLD_ARCHITECTURE_H__
#define __BFLD_ARCHITECTURE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "reloc.h"


enum arch_type
{
    ARCH_UNKNOWN,
    ARCH_x86_64,
    ARCH_AARCH64
};





/*
 * Architecture specific handling.
 */
struct arch_handler
{
    const char *name;

    enum arch_type arch_type;

    void (*create_plt_entry)(uint8_t *plt_content, uint64_t addr);

    size_t plt_entry_size;

    void (*setup_got_entry)(uint8_t *got_content, uint64_t addr);

    size_t got_entry_size;

    void (*apply_relocation)(const struct relocation *reloc, 
                             uint64_t base_addr,
                             uint8_t *content);
};



int arch_handler_register(const struct arch_handler *handler);


void arch_applu_relocation(const struct arch_handler *handler,
                           const struct relocation *reloc,
                           uint64_t base_addr,
                           uint8_t *content);



#define ARCH_HANDLER_INIT __attribute__((constructor))


#ifdef __cplusplus
}
#endif
#endif
