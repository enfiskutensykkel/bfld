#ifndef BFLD_UTILS_ARENA_ALLOCATOR_H
#define BFLD_UTILS_ARENA_ALLOCATOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <stdatomic.h>
#include "align.h"
#include "cdefs.h"


#define SLAB_SIZE   4096
#define SLAB_ALIGN  16


/*
 * Represents a slab of memory.
 */
struct slab
{
    struct slab *next;          // next slab pointer
    size_t size;                // data size in number of bytes
    _Atomic size_t used;        // number of bytes currently used
    alignas(SLAB_ALIGN) uint8_t data[]; // raw object data
};


/*
 * Allocate a memory slab.
 */
struct slab * slab_alloc(size_t size, size_t align);


/*
 * Release a memory slab.
 */
void slab_free(struct slab *slab);


/*
 * Arena allocator implementation.
 * 
 * An arena allocator makes it possible to
 * trivially grown allocated memory by adding 
 * slabs/blocks of more memory.
 */
struct arena
{
    struct slab * _Atomic head; // linked list of slabs
    size_t size;                // size of a slab (exclusive headers)
    size_t align;               // slab alignment
};


static inline
size_t arena_slab_size(const struct arena *arena)
{
    if (arena->size < SLAB_SIZE) {
        return SLAB_SIZE;
    }

    return arena->size;
}


static inline
size_t arena_slab_align(const struct arena *arena)
{
    if (arena->align < SLAB_ALIGN) {
        return SLAB_ALIGN;
    }

    return arena->align;
}


/*
 * 
 */
#define ARENA_INIT(size, align) \
    (struct arena) { NULL, ((size) + ((align) - 1)) & ~((align) - 1), (align) }


/*
 * Initialize an arena allocator.
 */
static inline
void arena_init(struct arena *arena, size_t slab_size, size_t slab_align)
{
    slab_align = align_roundup(slab_align);
    atomic_init(&arena->head, NULL);
    arena->size = align_to(slab_size, slab_align);
    arena->align = slab_align;
}


/*
 * Release all memory allocated by the arena allocator.
 *
 * This should only be called once the caller is sure
 * that previously allocated memory is no longer in use.
 */
static inline
void arena_destroy(struct arena *arena)
{
    struct slab *head = atomic_exchange_explicit(&arena->head, NULL, memory_order_acq_rel);

    while (head != NULL) {
        struct slab *next = head->next;
        slab_free(head);
        head = next;
    }
}


/*
 * Allocate slab memory.
 * 
 * Reserve at least size bytes in a memory slab and return a pointer to
 * the allocated memory. The memory is initialised to zero.
 *
 * The returned pointer will be aligned to the alignment given by align.
 *
 * The allocated memory is guaranteed to remain until the arena is destroyed.
 */
static inline
void * arena_alloc(struct arena *arena, size_t size, size_t align)
{
    size_t used, offset;
    uintptr_t base, addr;
    size_t aligned = align_to(size, align);
    struct slab *new_slab = NULL;

    for (;;) {
        struct slab *slab = atomic_load_explicit(&arena->head, memory_order_acquire);

        if (likely(slab != NULL)) {
            used = atomic_load_explicit(&slab->used, memory_order_acquire);
            base = (uintptr_t) slab->data;

            // Try to reserve size in the current slab
            for (;;) {
                addr = align_to(base + used, align);
                offset = (size_t) (addr - base);

                if (unlikely(offset + aligned > slab->size)) {
                    // Slab is full, there is no space
                    break;
                }

                if (atomic_compare_exchange_weak_explicit(&slab->used, &used, offset + aligned,
                            memory_order_acq_rel,
                            memory_order_acquire)) {
                    if (unlikely(new_slab != NULL)) {
                        slab_free(new_slab);
                    }
                    return &slab->data[offset];
                }
            }
        }

        // Current slab is full, let's try to allocate a new one
        if (new_slab == NULL) {
            size_t required = aligned + (align - 1);
            if (required < arena->size) {
                required = arena->size;
            }

            new_slab = slab_alloc(required, arena->align);
            if (unlikely(new_slab == NULL)) {
                return NULL;
            }

            base = (uintptr_t) new_slab->data;
            addr = align_to(base, align);
            offset = (size_t) (addr - base);
            atomic_store_explicit(&new_slab->used, offset + aligned, memory_order_relaxed);
        }

        new_slab->next = slab;

        // Try to insert the new slab into the arena
        if (atomic_compare_exchange_strong_explicit(&arena->head, &slab, new_slab,
                    memory_order_acq_rel,
                    memory_order_acquire)) {
            // We were able to insert the new slab into the arena
            base = (uintptr_t) new_slab->data;
            addr = align_to(base, align);
            offset = (size_t) (addr - base);
            return &new_slab->data[offset];
        }

        // All that work for nothing... we need to try again
    }
}


#ifdef __cplusplus
}
#endif
#endif
