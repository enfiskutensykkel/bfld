#ifndef BFLD_UTILS_HASH_H
#define BFLD_UTILS_HASH_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


static inline
uint32_t checksum_crc32(const uint8_t *data, size_t size)
{
    uint32_t table[256];
    table[0] = 0;

    uint32_t crc32 = 1;
    for (uint8_t i = 128; i > 0; i >>= 1) {
        crc32 = (crc32 >> 1) ^ (crc32 & 1 ? 0xedb88320 : 0);
        for (uint16_t j = 0; j < 256; j += 2*i) {
            table[i + j] = crc32 ^ table[j];
        }
    }

    crc32 = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc32 ^= data[i];
        crc32 = (crc32 >> 8) ^ table[crc32 & 0xff];
    }

    crc32 ^= 0xffffffffu;
    return crc32;
}


/*
 * Bernstein's DJB2 hash
 */
static inline
uint32_t hash_djb2(const char *str)
{
    uint32_t hash = 5381;
    uint8_t c;

    while ((c = (uint8_t) *str++) != '\0') {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}


/*
 * Fowler-Noll-Vo hash (FNV-1a).
 */
static inline
uint32_t hash_fnv1a(const char *str)
{
    uint32_t hash = 2166136261u;
    uint8_t c;

    while ((c = (uint8_t) *str++) != '\0') {
        hash ^= c;
        hash *= 16777619u;
    }
    
    return hash;
}


#ifdef __cplusplus
}
#endif
#endif
