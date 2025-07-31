#ifndef __BFLD_RELOCATION_H__
#define __BFLD_RELOCATION_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


struct relocation
{
    uint32_t type;          // relocation type
    uint64_t addr;          // symbol address (finalized)
    uint64_t offset;        // offset within section
    int64_t addend;         // relocation addend (if applicable)
};


#ifdef __cplusplus
}
#endif
#endif
