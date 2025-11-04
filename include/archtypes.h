#ifndef __BFLD_ARCHITECTURE_TYPES_H__
#define __BFLD_ARCHITECTURE_TYPES_H__
#ifdef __cplusplus
extern "C" {
#endif


/*
 * Supported architectures
 */
enum arch_type
{
    ARCH_UNKNOWN,
    ARCH_X86_64,
    ARCH_AARCH64
};


/* Forward declaration */
struct arch_handler;


#ifdef __cplusplus
}
#endif
#endif
