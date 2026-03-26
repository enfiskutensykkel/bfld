#ifndef BFLD_UTILS_SPIN_LOCK_H
#define BFLD_UTILS_SPIN_LOCK_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <threads.h>
#include "cdefs.h"


/*
 * Spin lock implementation, which can be used to implement, for example,
 * a mutex, where only one thread can enter a critical section at the time.
 *
 * Uses C11 atomic compare-and-swap to implement a spin lock.
 */
struct spinlock
{
    atomic_uint_fast32_t value;
};


#define SPINLOCK_INIT (struct spinlock) {0}


/*
 * Initialize a spinlock.
 */
static inline 
void spinlock_init(struct spinlock *lock)
{
    atomic_store_explicit(&lock->value, 0, memory_order_relaxed);
}


/*
 * Acquire the spinlock and enter the critical section.
 */
static inline
void spinlock_lock(struct spinlock *lock)
{
    uint_fast32_t expected = 0;

    while (!atomic_compare_exchange_weak_explicit(&lock->value, &expected, 1,
                                                  memory_order_acquire,
                                                  memory_order_relaxed)) {
        expected = 0;
#if defined(__x86_64__) || defined(__i386__)
        __asm__ __volatile__("pause");
#elif defined(__aarch64__)
        __asm__ __volatile__("yield");
#else
        thrd_yield();
#endif
    }
}


/*
 * Release the spinlock and leave the critical section.
 */
static inline
void spinlock_unlock(struct spinlock *lock)
{
    atomic_store_explicit(&lock->value, 0, memory_order_release);
}


/*
 * Reader/writer lock implementation usinc C11 atomics.
 *
 * Allows multiple reader threads to access a shared resource simultaneously,
 * but only a single writer may modify the resource at the time.
 */
struct rwlock
{
    /*
     * Bit  0-15: Number of active readers  (mask 0x0000ffffUL)
     * Bit 16-30: Number of waiting writers (mask 0x7fff0000UL)
     * Bit    31: Writer active             (mask 0x80000000UL)
     */
    uint32_t _Atomic value;
};


#define RWLOCK_INIT (struct rwlock) {0}


/*
 * Initialize the reader/writer lock.
 */
static inline
void rwlock_init(struct rwlock *lock)
{
    atomic_store_explicit(&lock->value, 0, memory_order_relaxed);
}


/*
 * Acquire the read lock.
 *
 * This implementation is reader preferring (reader biased). 
 * It will continue to admit readers even if there is a writer waiting to 
 * take the writer lock. Note that this may lead to writer starvation 
 * if there are many readers and high contention for the lock, but is
 * faster than the writer preferring version.
 */
static inline
void rwlock_biased_read_lock(struct rwlock *lock)
{
    uint32_t expected = atomic_load_explicit(&lock->value, memory_order_acquire);

    for (;;) {
        if (unlikely(expected & 0x80000000UL)) {
#if defined(__x86_64__) || defined(__i386__)
            __asm__ __volatile__("pause");
#elif defined(__aarch64__)
            __asm__ __volatile__("yield");
#else
            thrd_yield();
#endif
            expected = atomic_load_explicit(&lock->value, memory_order_acquire);
            continue;
        }

        uint32_t next = (expected & 0x0000ffffUL) + 1;
        if (atomic_compare_exchange_weak(&lock->value, &expected, next,
                                         memory_order_acquire,
                                         memory_order_relaxed)) {
            break;
        }
    }
}


/*
 * Acquire the read lock.
 */
static inline
void rwlock_read_lock(struct rwlock *lock)
{
    uint32_t expected = atomic_load_explicit(&lock->value, memory_order_acquire);

    for (;;) {
        if (unlikely(expected & 0xffff0000UL)) {
#if defined(__x86_64__) || defined(__i386__)
            __asm__ __volatile__("pause");
#elif defined(__aarch64__)
            __asm__ __volatile__("yield");
#else
            thrd_yield();
#endif
            expected = atomic_load_explicit(&lock->value, memory_order_acquire);
            continue;
        }

        uint32_t next = (expected & 0x0000ffffUL) + 1;
        if (atomic_compare_exchange_weak(&lock->value, &expected, next,
                                         memory_order_acquire,
                                         memory_order_relaxed)) {
            break;
        }
    }
}


/*
 * Release the read lock.
 */
static inline
void rwlock_read_unlock(struct rwlock *lock)
{
    atomic_fetch_sub_explicit(&lock->value, 1, memory_order_release);
}


/*
 * Acquire the writer lock and enter critical section.
 */
static inline
void rwlock_write_lock(struct rwlock *lock)
{
    uint32_t expected = atomic_load_explicit(&lock->value, memory_order_acquire);

    for (;;) {
        if (likely(expected & 0x8000ffffUL)) {
#if defined(__x86_64__) || defined(__i386__)
            __asm__ __volatile__("pause");
#elif defined(__aarch64__)
            __asm__ __volatile__("yield");
#else
            thrd_yield();
#endif
            expected = atomic_load_explicit(&lock->value, memory_order_acquire);
            continue;
        }

        uint32_t value = expected | 0x80000000;
        if (atomic_compare_exchange_weak_explicit(&lock->value, &expected, value,
                                                  memory_order_acquire,
                                                  memory_order_relaxed)) {
            break;
        }
    }
}


/*
 * Acquire the writer lock and enter critical section.
 *
 * This implementation is writer preferring (writer biased). 
 * Note that this may lead to reader starvation.
 */
static inline
void rwlock_biased_write_lock(struct rwlock *lock)
{
    uint32_t expected = 0;

    // Try to take writer lock directly
    if (atomic_compare_exchange_strong_explicit(&lock->value, &expected, 0x80000000UL,
                                                memory_order_acquire,
                                                memory_order_relaxed)) {
        return;
    }

    // We have to wait for our turn, signal that we are waiting
    atomic_fetch_add_explicit(&lock->value, 0x00010000UL, memory_order_release);
    expected = atomic_load_explicit(&lock->value, memory_order_acquire);

    for (;;) {
        if (likely(expected & 0x8000ffffUL)) {
#if defined(__x86_64__) || defined(__i386__)
            __asm__ __volatile__("pause");
#elif defined(__aarch64__)
            __asm__ __volatile__("yield");
#else
            thrd_yield();
#endif
            expected = atomic_load_explicit(&lock->value, memory_order_acquire);
            continue;
        }

        uint32_t next = (expected - 0x00010000UL) | 0x80000000UL;
        if (atomic_compare_exchange_weak_explicit(&lock->value, &expected, next,
                                                  memory_order_acquire,
                                                  memory_order_relaxed)) {
            break;
        }
    }
}


/*
 * Release writer lock and leave critical section.
 */
static inline
void rwlock_write_unlock(struct rwlock *lock)
{
    atomic_fetch_and_explicit(&lock->value, ~0x80000000UL, memory_order_release);
}


#ifdef __cplusplus
}
#endif
#endif
