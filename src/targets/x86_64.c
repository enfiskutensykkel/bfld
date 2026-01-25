#include "utils/bswap.h"
#include "target.h"
#include <elf.h>
#include "logging.h"
#include <errno.h>


static int reloc_abs64(uint8_t *sect, uint64_t offs, uint64_t base, uint64_t target, int64_t addend)
{
    uint64_t value = target + addend;
    write_le64(sect + offs, value);
    return 0;
}


static int reloc_pc32(uint8_t *sect, uint64_t offs, uint64_t base, uint64_t target, int64_t addend)
{
    uint64_t pc = base + offs + 4;
    int64_t value = (target + addend) - pc;

    if (value < INT32_MIN || value > INT32_MAX) {
        log_error("Relocation overflow: : R_X86_64_PC32 value %ld out of range", value);
        return ERANGE;
    }

    write_le32(sect + offs, (int32_t) value);
    return 0;
}


static int reloc_abs32(uint8_t *sect, uint64_t offs, uint64_t base, uint64_t target, int64_t addend)
{
    uint64_t value = target + addend;

    if (value > UINT32_MAX) {
        log_error("Relocation overflow: R_X86_64_32 value %lu exceeds 32 bits", value);
        return ERANGE;
    }

    write_le32(sect + offs, (uint32_t) value);
    return 0;
}


static int reloc_abs32s(uint8_t *sect, uint64_t offs, uint64_t base, uint64_t target, int64_t addend)
{
    int64_t value = target + addend;
    write_le32(sect + offs, (int32_t) value);
    return 0;
}


static int (*table[256])(uint8_t*, uint64_t, uint64_t, uint64_t, int64_t) = {
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
        log_fatal("Unknown relocation type 0x%02x", reloc_type);
        return EINVAL;
    }

    int status = table[reloc_type](sect, offset, baseaddr, targetaddr, addend);
    if (status != 0) {
        log_fatal("Unexpected relocation error");
        return status;
    }

    return 0;
}


const struct target x86_64_target = {
    .name = "x86-64",
    .section_boundary = 4096,
    .cpu_align = 16,
    .min_page_size = 4096,
    .max_page_size = 4096,
    .apply_reloc = x86_64_apply_reloc
};


__attribute__((constructor))
static void x86_64_target_init(void)
{
    target_register(&x86_64_target, EM_X86_64);
}
