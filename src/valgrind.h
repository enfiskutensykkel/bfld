#ifndef BFLD_VALGRIND_HELPER_H
#define BFLD_VALGRIND_HELPER_H

#if !defined(NDEBUG) && defined(HAS_VALGRIND)
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MALLOCLIKE_BLOCK(addr, size, redzone_bytes, is_zeroed) (void) 0
#define VALGRIND_FREELIKE_BLOCK(addr, redzone_bytes) (void) 0
#define VALGRIND_MAKE_MEM_NOACCESS(addr, len) (void) 0
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, len) (void) 0
#define VALGRIND_MAKE_MEM_DEFINED(addr, len) (void) 0
#endif

#endif
