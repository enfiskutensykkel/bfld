#include "arena.h"
#include "align.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>

#if defined(HAS_VALGRIND) && !defined(NDEBUG)
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MALLOCLIKE_BLOCK(addr, size, redzone_bytes, is_zeroed) (void) 0
#define VALGRIND_FREELIKE_BLOCK(addr, redzone_bytes) (void) 0
#define VALGRIND_MAKE_MEM_NOACCESS(addr, len) (void) 0
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, len) (void) 0
#endif


struct slab * slab_alloc(size_t size, size_t align)
{
    if (align < SLAB_ALIGN) {
        align = SLAB_ALIGN;
    }
    align = align_roundup(align);

    if (size < SLAB_SIZE) {
        size = SLAB_SIZE;
    }

    size_t msize = align_to(sizeof(struct slab) + size, align);
    
    // mmap is usually slower than malloc and causes a context switch/syscall
    // however, we always get page-aligned memory (which is nice), zeroed out memory,
    // and "lazy" loaded memory (faulted in only when accessed)
    struct slab *slab = mmap(NULL, msize, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS /*| MAP_POPULATE*/, -1, 0);
    if (slab == MAP_FAILED) {
        return NULL;
    }

    VALGRIND_MALLOCLIKE_BLOCK(slab, msize, 0, 1);
    VALGRIND_MAKE_MEM_UNDEFINED(slab, sizeof(struct slab));
    VALGRIND_MAKE_MEM_NOACCESS(slab->data, msize - sizeof(struct slab));

    uint64_t base = (uint64_t) slab->data;
    uint64_t addr = align_to(base, align);
    size_t offs = (size_t) (addr - base);

    slab->next = NULL;
    slab->size = msize - sizeof(struct slab);
    atomic_init(&slab->used, offs);

    return slab;
}


void slab_free(struct slab *slab)
{
    if (slab != NULL) {
        size_t size = slab->size + sizeof(struct slab);
        VALGRIND_FREELIKE_BLOCK(slab, 0);
        munmap(slab, size);
    }
}

// mmap with map_private | map_anonymous for allocating array
//atomic_uint_fast32_t next_slot; // next object identifier
//_Atomic(void *) * index;        // object index for fast object lookup

