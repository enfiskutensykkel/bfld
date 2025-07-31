#include "utils/bswap.h"
#include "x86_64.h"
#include <arch.h>
#include <reloc.h>


static void reloc_abs64(const struct relocation *reloc, uint64_t base_addr, uint8_t *output)
{
    uint64_t value = reloc->symbol_addr + reloc->addend;
    write_le64(output + reloc->offset, value);
}


static void reloc_pc32(const struct relocation *reloc, uint64_t base_addr, uint8_t *output)
{
    uint64_t pc = base_addr + reloc->offset + 4;
    int64_t value = (reloc->symbol_addr + reloc->addend) - pc;
    write_le32(output + reloc->offset, (int32_t) value);
}


static void reloc_abs32(const struct relocation *reloc, uint64_t base_addr, uint8_t *output)
{
    uint64_t value = reloc->symbol_addr + reloc->addend;
    write_le32(output + reloc->offset, (uint32_t) value);
}


static void reloc_abs32s(const struct relocation *reloc, uint64_t base_addr, uint8_t *output)
{
    int64_t value = reloc->symbol_addr + reloc->addend;

    write_le32(output + reloc->offset, (int32_t) value);
}


static void (*table[256])(const struct relocation *reloc, uint64_t addr, uint8_t *output) = {
    [RELOC_X86_64_ABS64] = reloc_abs64,
    [RELOC_X86_64_PC32] = reloc_pc32,
    [RELOC_X86_64_ABS32] = reloc_abs32,
    [RELOC_X86_64_ABS32S] = reloc_abs32s,
};


static void relocation(const struct relocation *reloc, uint64_t base_addr, uint8_t *output)
{
    if (table[reloc->type] != NULL) {
        table[reloc->type](reloc, base_addr, output);
    }
}


const struct arch_handler x86_64_handler = {
    .name = "x86-64",
    .apply_relocation = relocation
};


ARCH_HANDLER_INIT void arch_x86_64_init(void)
{
    arch_handler_register(&x86_64_handler);
}
