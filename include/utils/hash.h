#ifndef BFLD_UTILS_HASH_H
#define BFLD_UTILS_HASH_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>


static inline
uint32_t checksum_crc32(const void *data, size_t size)
{
    uint32_t table[256];
    const uint8_t *bytes = (const uint8_t*) data;
    table[0] = 0;

    uint32_t crc32 = 1;
    for (uint16_t i = 128; i > 0; i >>= 1) {
        crc32 = (crc32 >> 1) ^ (crc32 & 1 ? 0xedb88320 : 0);
        for (uint16_t j = 0; j < 256; j += 2*i) {
            table[i + j] = crc32 ^ table[j];
        }
    }

    crc32 = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc32 ^= bytes[i];
        crc32 = (crc32 >> 8) ^ table[crc32 & 0xff];
    }

    crc32 ^= 0xffffffffu;
    return crc32;
}


/*
 * Daniel J. Bernstein's DJB2 hash
 */
static inline
uint32_t hash_djb2_32(const void *data, size_t size)
{
    uint32_t hash = 5381;
    const uint8_t *bytes = (const uint8_t*) data;

    for (size_t i = 0; i < size; ++i) {
        hash = ((hash << 5) + hash) + bytes[i];
    }

    return hash;
}


/*
 * Daniel J. Bernstein's DJB2 hash
 */
static inline
uint64_t hash_djb2_64(const void *data, size_t size)
{
    uint64_t hash = 5381;
    const uint8_t *bytes = (const uint8_t*) data;

    for (size_t i = 0; i < size; ++i) {
        hash = ((hash << 5) + hash) + bytes[i];
    }

    return hash;
}


/*
 * Fowler-Noll-Vo hash (FNV-1a).
 */
static inline
uint32_t hash_fnv1a_32(const void *data, size_t size)
{
    uint32_t hash = 0x811c9dc5UL;
    const uint8_t *bytes = (const uint8_t*) data;

    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 0x01000193UL;
    }
    
    return hash;
}


/*
 * Fowler-Noll-Vo hash (FNV-1a).
 */
static inline
uint64_t hash_fnv1a_64(const void *data, size_t size)
{
    uint64_t hash = 0xcbf29ce484222325ULL;
    const uint8_t *bytes = (const uint8_t*) data;

    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 0x100000001b3;
    }
    
    return hash;
}


/*
 * Helper function to rotate an uint32 left by r.
 */
static inline 
uint32_t hash_rotate_left_32(uint32_t value, int r)
{
    return (value << r) | (value >> (32 - r));
}


/*
 * Helper function to rotate an uint64 left by r.
 */
static inline 
uint64_t hash_rotate_left_64(uint64_t value, int r)
{
    return (value << r) | (value >> (64 - r));
}


/*
 * My attempt at Yann Collett's XXH32 hash.
 *
 * Based on the description at https://xxhash.com/ and Stefan Brumme's
 * C++ implementation: https://create.stephan-brumme.com/xxhash/
 */
static inline
uint32_t hash_xxh_32(const void *data, size_t size)
{
    const uint32_t p0 = 2654435761U;
    const uint32_t p1 = 2246822519U;
    const uint32_t p2 = 3266489917U;
    const uint32_t p3 = 668265263U;
    const uint32_t p4 = 374761393U;

    // FIXME: data might be unaligned, maybe use memcpy instead
    size_t i = 0;
    uint32_t hash;

    if (size >= 16) {
        uint32_t v[] = {p0 + p1, p1, 0, 0 - p0};

        do {
            // FIXME: unroll this if the compiler doesn't
            for (int j = 0; j < 4; ++j) {
                uint32_t x = *((const uint32_t*) (((const char*) data) + i));
                uint32_t vc = v[j];
                vc += x * p1;
                vc = hash_rotate_left_32(vc, 13);
                vc *= p0;
                v[j] = vc;
                i += 4;
            }
        } while (i <= size - 16);

        hash = hash_rotate_left_32(v[0], 1) +
               hash_rotate_left_32(v[1], 7) + 
               hash_rotate_left_32(v[2], 12) +
               hash_rotate_left_32(v[3], 18);

    } else {
        hash = p4;
    }

    hash += (uint32_t) size;

    while (i + 4 <= size) {
        uint32_t x = *((const uint32_t*) (((const char*) data) + i));
        hash += x * p2;
        hash = hash_rotate_left_32(hash, 17) * p3;
        i += 4;
    }

    while (i < size) {
        uint8_t x = *(((const uint8_t*) data) + i);
        hash += x * p4;
        hash = hash_rotate_left_32(hash, 11) * p0;
        ++i;
    }

    hash ^= hash >> 15;
    hash *= p1;
    hash ^= hash >> 13;
    hash *= p2;
    hash ^= hash >> 16;
    return hash;
}


/*
 * My attempt at Yan Collet's XXH64 hash.
 *
 * Based on the description at https://xxhash.com/
 * and whatever is in Gemini's training set.
 */
static inline
uint64_t hash_xxh_64(const void *data, size_t size)
{
    const uint64_t p0 = 11400714785074694791ULL;
    const uint64_t p1 = 14029467366897019727ULL;
    const uint64_t p3 = 1609587929392839161ULL;
    const uint64_t p4 = 9650029242287828579ULL;
    const uint64_t p5 = 2870177450012600261ULL;

    // FIXME: data might be unaligned, maybe use memcpy instead
    size_t i = 0;
    uint64_t hash;

    if (size >= 32) {
        uint64_t v[] = {p0 + p1, p1, 0, 0 - p0};

        do {
            // FIXME: unroll this if the compiler doesn't do it automatically
            for (int j = 0; j < 4; ++j) {
                uint64_t x = *((const uint64_t*) (((const char*) data) + i));
                uint64_t vc = v[j];
                vc += x * p1;
                vc = hash_rotate_left_64(vc, 31);
                vc *= p0;
                v[j] = vc;
                i += 8;
            }
        } while (i <= size - 32);

        hash = hash_rotate_left_64(v[0], 1) +
               hash_rotate_left_64(v[1], 7) +
               hash_rotate_left_64(v[2], 12) + 
               hash_rotate_left_64(v[3], 18);

        for (int j = 0; j < 4; ++j) {
            uint64_t m = hash_rotate_left_64(v[j] * p1, 31) * p0;
            hash ^= m;
            hash = hash * p0 + p4;
        }
    } else {
        hash = p5;
    }

    hash += (uint64_t) size;

    while (i + 8 <= size) {
        uint64_t x = *((const uint64_t*) (((const char*) data) + i));
        uint64_t k = hash_rotate_left_64(x * p1, 31) * p0;
        hash ^= k;
        hash = hash_rotate_left_64(hash, 27) * p0 + p4;
        i += 8;
    }

    if (i + 4 <= size) {
        uint32_t x = *((const uint32_t*) (((const char*) data) + i));
        hash ^= x * p0;
        hash = hash_rotate_left_64(hash, 23) * p1 + p3;
        i += 4;
    }

    while (i < size) {
        uint8_t x = *(((const uint8_t*) data) + i);
        hash ^= x * p5;
        hash = hash_rotate_left_64(hash, 11) * p0;
        ++i;
    }

    hash ^= hash >> 33;
    hash *= p1;
    hash ^= hash >> 29;
    hash *= p3;
    hash ^= hash >> 32;

    return hash;
}



/*
 * SplitMix64 hash for pointer values.
 */
static inline
uint64_t hash_pointer(const void *ptr)
{
    uint64_t hash = (uint64_t) ptr;
    hash = (hash ^ (hash >> 30)) * 0xbf58476d1ce4e5b9ULL;
    hash = (hash ^ (hash >> 27)) * 0x94d049bb133111ebULL;
    hash = hash ^ (hash >> 31);
    return hash;
}


/*
 * Generic golden ratio hash.
 */
//static inline
//uint32_t hash_64(uint64_t value, uint8_t bits)
//{
//    return value * 0x61c8864680b583ebULL >> (64 - bits);
//}
//
//
//static inline
//uint32_t hash_32(uint32_t value, uint8_t bits)
//{
//    return (value * 0x61c88647) >> (32 - bits);
//}


#ifdef __cplusplus
}
#endif
#endif
