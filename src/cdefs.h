#ifndef BFLD_C_DEFINES_H
#define BFLD_C_DEFINES_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_NO_ATOMICS__
#error "Atomic operations are required"
#endif

/*
 * Deal with C11 atomics stuff
 */
#if defined(HAS_C11_ATOMICS) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) \
    || (defined(__has_include) && __has_include(<stdatomic.h>))

#include <stdatomic.h>

#elif defined(HAS_BUILTIN_ATOMICS) || (defined(__GNUC__) || defined(__clang__))

#define _Atomic(T) T

#define memory_order_relaxed __ATOMIC_RELAXED
#define memory_order_acquire __ATOMIC_ACQUIRE
#define memory_order_release __ATOMIC_RELEASE
#define memory_order_acq_rel __ATOMIC_ACQ_REL
#define memory_order_seq_cst __ATOMIC_SEQ_CST

#define atomic_init(ptr, val) \
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST)

#define atomic_store_explicit(ptr, val, order) \
    __atomic_store_n(ptr, val, order)

#define atomic_store(ptr, val) \
    atomic_store_explicit(ptr, val, __ATOMIC_SEQ_CST)

#define atomic_load_explicit(ptr, order) \
    __atomic_load_n(ptr, order)

#define atomic_load(ptr) \
    atomic_load_explicit(ptr, __ATOMIC_SEQ_CST)

#define atomic_exchange_explicit(ptr, val, order) \
    __atomic_exchange_n(ptr, val, order)

#define atomic_exchange(ptr, val) \
    atomic_exchange_explicit(ptr, val, __ATOMIC_SEQ_CST)

#define atomic_fetch_add_explicit(ptr, val, order) \
    __atomic_fetch_add(ptr, val, order)

#define atomic_fetch_add(ptr, val) \
    atomic_fetch_add_explicit(ptr, val, __ATOMIC_SEQ_CST)

#define atomic_fetch_sub_explicit(ptr, val, order) \
    __atomic_fetch_sub(ptr, val, order)

#define atomic_fetch_sub(ptr, val) \
    atomic_fetch_sub_explicit(ptr, val, __ATOMIC_SEQ_CST)

#define atomic_fetch_and_explicit(ptr, val, order) \
    __atomic_fetch_and(ptr, val, order)

#define atomic_fetch_and(ptr, val) \
    __atomic_fetch_and(ptr, val, __ATOMIC_SEQ_CST)

#define atomic_fetch_or_explicit(ptr, val, order) \
    __atomic_fetch_or(ptr, val, order)

#define atomic_fetch_or(ptr, val) \
    __atomic_fetch_or(ptr, val, __ATOMIC_SEQ_CST)

#define atomic_fetch_xor_explicit(ptr, val, order) \
    __atomic_fetch_xor(ptr, val, order)

#define atomic_fetch_xor(ptr, val) \
    __atomic_fetch_xor(ptr, val, __ATOMIC_SEQ_CST)

#define atomic_compare_exchange_strong_explicit(ptr, exp, des, suc, fail) \
    __atomic_compare_exchange_n(ptr, exp, des, 0, suc, fail)

#define atomic_compare_exchange_strong(ptr, exp, des) \
    atomic_compare_exchange_strong_explicit(ptr, exp, des, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

#define atomic_compare_exchange_weak_explicit(ptr, exp, des, suc, fail) \
    __atomic_compare_exchange_n(ptr, exp, des, 1, suc, fail)

#define atomic_compare_exchange_weak(ptr, exp, des) \
    __atomic_compare_exchange_n(ptr, exp, des, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

#endif


/*
 * Define some convenience macros.
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && (defined(__GNUC__) || defined(__clang__))

#undef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)

#define __same_type(a, b) __builtin_types_compatible_p(__typeof__(a), __typeof__(b))


/*
 * Cast member of a structure out to the containing structure.
 * This is unsafe, use below instead.
 */
#define __containerof(ptr, type, member) __extension__ ({ \
        void *__mptr = (void *)(ptr); \
        _Static_assert(__same_type(*(ptr), ((type *)0)->member) || \
                __same_type(*(ptr), void), \
                "pointer type mismatch in containerof()");	\
                ((type *)((char*) __mptr - offsetof(type, member))); })

/*
 * Cast member of a structure out to the containing structure.
 * This preserves const-correctness.
 */
#define containerof(ptr, type, member) \
    _Generic(ptr, \
            const __typeof__(*(ptr)) *: ((const type *) __containerof(ptr, type, member)), \
            default: ((type *) __containerof(ptr, type, member)) \
            )


/*
 * Expect the condition to be true
 */
#define likely(x)   __builtin_expect(!!(x), 1)

/*
 * Expect the condition to be false
 */
#define unlikely(x) __builtin_expect(!!(x), 0)


#else

#undef offsetof
#define offsetof(type, member) \
    ((size_t) &(((type*) 0)->member))


/*
 * Cast member of a structure out to the containing structure.
 * This is type unsafe and does not preserve const correctness.
 */
#define __containerof(ptr, type, member) \
    ((type*) ((char*) ((void*) ptr) - offsetof(type, member)))


#define containerof(ptr, type, member) __containerof(ptr, type, member)

#define likely(x)   !!(x)

#define unlikely(x) !!(x)

#endif


/*
 * Thread local definitions.
 */
#if defined(HAS_C11_THREADS) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) \
    || (defined(__has_include) && __has_include(<threads.h>))

#include <threads.h>

#elif defined(__GNUC__) || defined(__clang__)

#define _Thread_local __thread

#endif


/*
 * Thread yield macro.
 */
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
#define thread_pause()  __asm__ __volatile__("pause")

#elif (defined(__GNUC__) || defined(__clang__)) && defined(__aarch64__)
#define thread_pause() __asm__ __volatile__("yield")

#elif defined(HAS_INTEL_INTRINSICS)
#include <immintrin.h>
#define thread_pause()  _mm_pause()

#elif HAS_C11_THREADS
#include <threads.h>
#define thread_pause() thrd_yield()

#elif defined(HAS_SCHED_YIELD) 
#include <sched.h>
#define thread_pause() sched_yield()

#else
#define thread_pause() (void) 0

#endif

#ifdef __cplusplus
}
#endif


#if defined(__cplusplus)
#include <atomic>

#define _Atomic(T) std::atomic<T>
//#define _Thread_local thread_local

using std::memory_order_relaxed;
using std::memory_order_release;
using std::memory_order_acquire;
using std::memory_order_acq_rel;
using std::memory_order_seq_cst;

#endif


#endif
