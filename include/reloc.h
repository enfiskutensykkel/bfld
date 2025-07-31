#ifndef __BFLD_RELOCATION_H__
#define __BFLD_RELOCATION_H__
#ifdef __cplusplus
extern "C" {
#endif


/* Forward declaration of symbol */
struct symbol;


struct relocation
    uint64_t offset;        // offset within section
    struct symbol *symbol;  // the symbol the relocation applies for (target symbol)
};


#ifdef __cplusplus
}
#endif
#endif
