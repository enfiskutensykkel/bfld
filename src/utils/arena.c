#include "arena.h"
#include "align.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>

#if defined(HAS_VALGRIND) && !defined(NDEBUG)
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MALLOCLIKE_BLOCK(addr, size, redzone_bytes, is_zeroed) (void) 0
#define VALGRIND_FREELIKE_BLOCK(addr, redzone_bytes) (void) 0
#define VALGRIND_MAKE_MEM_NOACCESS(addr, len) (void) 0
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, len) (void) 0
#endif


void region_free(struct region *region)
{
    VALGRIND_FREELIKE_BLOCK(region->data, 0);
    munmap(region->data, region->size);
    free(region);
}


struct region * region_alloc(size_t size)
{
    size = align_to(size, PAGE_SIZE);

    if (size >= REGION_SIZE_MAX) {
        return NULL;
    }

    struct region *region = malloc(sizeof(struct region));
    if (region == NULL) {
        return NULL;
    }

    void *memory = mmap(NULL, size, PROT_READ | PROT_WRITE, 
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        free(region);
        return NULL;
    }

    VALGRIND_MALLOCLIKE_BLOCK(memory, size, 0, 1);
    VALGRIND_MAKE_MEM_UNDEFINED(memory, size);

    atomic_init(&region->next, NULL);
    atomic_init(&region->used, 0);
    region->size = size;
    region->data = memory;

    return region;
}
