#include "arena.h"
#include "align.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>


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
    struct slab *slab = mmap(NULL, msize, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (slab == MAP_FAILED) {
        return NULL;
    }

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
    munmap(slab, slab->size + sizeof(struct slab));
}

// mmap with map_private | map_anonymous for allocating array
//atomic_uint_fast32_t next_slot; // next object identifier
//_Atomic(void *) * index;        // object index for fast object lookup
// uint32_t nslots;                // total number of object slots
// align_to(min_size, 16)

//static inline


