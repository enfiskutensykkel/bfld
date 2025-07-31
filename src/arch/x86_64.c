#include "x86_64.h"
#include <arch.h>


static void relocation(uint64_t addr, uint8_t *content, uint64_t offset, uint32_t type, int64_t addend)
{
}


const struct arch_handler x86_64_handler = {
    .name = "x86-64",
    .emit_relocation = relocation
};


ARCH_HANDLER_INIT void arch_x86_64_init(void)
{
    arch_handler_register(&x86_64_handler);
}
