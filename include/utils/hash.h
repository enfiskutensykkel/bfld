#ifndef BFLD_UTILS_HASH_H
#define BFLD_UTILS_HASH_H
#ifdef __cplusplus
extern "C" {
#endif

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
