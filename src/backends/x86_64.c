#include "utils/bswap.h"
#include "backend.h"
#include <elf.h>
#include "logging.h"
#include <errno.h>


static void reloc_abs64(uint8_t *sect, uint64_t offs, uint64_t base, uint64_t target, int64_t addend)
{
    uint64_t value = target + addend;
    write_le64(sect + offs, value);
}


static void reloc_pc32(uint8_t *sect, uint64_t offs, uint64_t base, uint64_t target, int64_t addend)
{
    uint64_t pc = base + offs + 4;
    int64_t value = (target + addend) - pc;
    write_le32(sect + offs, (int32_t) value);
}


static void reloc_abs32(uint8_t *sect, uint64_t offs, uint64_t base, uint64_t target, int64_t addend)
{
    uint64_t value = target + addend;
    write_le32(sect + offs, (uint32_t) value);
}


static void reloc_abs32s(uint8_t *sect, uint64_t offs, uint64_t base, uint64_t target, int64_t addend)
{
    int64_t value = target + addend;
    write_le32(sect + offs, (int32_t) value);
}


static void (*table[256])(uint8_t*, uint64_t, uint64_t, uint64_t, int64_t) = {
    [R_X86_64_64] = reloc_abs64,
    [R_X86_64_PC32] = reloc_pc32,
    [R_X86_64_32] = reloc_abs32,
    [R_X86_64_32S] = reloc_abs32s,
    [R_X86_64_PLT32] = reloc_pc32,
};


static int x86_64_apply_reloc(uint8_t *sect, uint64_t offset,
                              uint64_t baseaddr, uint64_t targetaddr,
                              int64_t addend, uint32_t reloc_type)
{
    if (table[reloc_type] == NULL) {
        log_fatal("Unknown reloc type 0x%02x", reloc_type);
        return EINVAL;
    }

    table[reloc_type](sect, offset, baseaddr, targetaddr, addend);
    return 0;
}


const struct backend x86_64_be = {
    .name = "x86-64",
    .march = EM_X86_64,
    .march_align = 16,
    .apply_reloc = x86_64_apply_reloc
};


__attribute__((constructor))
static void x86_64_be_init(void)
{
    backend_register(&x86_64_be);
}
