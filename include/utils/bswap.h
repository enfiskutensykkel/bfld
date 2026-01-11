#ifndef BFLD_UTILS_BSWAP_H
#define BFLD_UTILS_BSWAP_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


static inline
void write_le16(uint8_t *ptr, uint16_t value)
{
    ptr[0] = value & 0xff;
    ptr[1] = (value >> 8) & 0xff;
}


static inline
void write_le32(uint8_t *ptr, uint32_t value)
{
    ptr[0] = value & 0xff;
    ptr[1] = (value >> 8) & 0xff;
    ptr[2] = (value >> 16) & 0xff;
    ptr[3] = (value >> 24) & 0xff;
}


static inline
void write_le64(uint8_t *ptr, uint64_t value)
{
    ptr[0] = value & 0xff;
    ptr[1] = (value >> 8) & 0xff;
    ptr[2] = (value >> 16) & 0xff;
    ptr[3] = (value >> 24) & 0xff;
    ptr[4] = (value >> 32) & 0xff;
    ptr[5] = (value >> 40) & 0xff;
    ptr[6] = (value >> 48) & 0xff;
    ptr[7] = (value >> 56) & 0xff;
}


static inline
void write_be16(uint8_t *ptr, uint16_t value) 
{
    ptr[0] = (value >> 8) & 0xff;
    ptr[1] = value & 0xff;
}


static inline
void write_be32(uint8_t *ptr, uint32_t value) 
{
    ptr[0] = (value >> 24) & 0xff;
    ptr[1] = (value >> 16) & 0xff;
    ptr[2] = (value >> 8) & 0xff;
    ptr[3] = value & 0xff;
}


static inline
void write_be64(uint8_t *ptr, uint64_t value) 
{
    ptr[0] = (value >> 56) & 0xff;
    ptr[1] = (value >> 48) & 0xff;
    ptr[2] = (value >> 40) & 0xff;
    ptr[3] = (value >> 32) & 0xff;
    ptr[4] = (value >> 24) & 0xff;
    ptr[5] = (value >> 16) & 0xff;
    ptr[6] = (value >> 8) & 0xff;
    ptr[7] = value & 0xff;
}


static inline
uint16_t read_le16(const uint8_t *ptr)
{
    return ((uint16_t) ptr[0]) | ((uint16_t) ptr[1] << 8);
}


static inline
uint32_t read_le32(const uint8_t *ptr)
{
    return ((uint32_t) ptr[0]) | 
           ((uint32_t) ptr[1] << 8) |
           ((uint32_t) ptr[2] << 16) |
           ((uint32_t) ptr[3] << 24);
}


static inline
uint64_t read_le64(const uint8_t *ptr)
{
    return ((uint64_t) ptr[0]) | 
           ((uint64_t) ptr[1] << 8) |
           ((uint64_t) ptr[2] << 16) |
           ((uint64_t) ptr[3] << 24) |
           ((uint64_t) ptr[4] << 32) |
           ((uint64_t) ptr[5] << 40) |
           ((uint64_t) ptr[6] << 48) |
           ((uint64_t) ptr[7] << 56);
}


static inline
uint16_t read_be16(const uint8_t *ptr)
{
    return ((uint16_t) ptr[0] << 8) | (uint16_t) ptr[1];
}


static inline
uint32_t read_be32(const uint8_t *ptr)
{
    return ((uint32_t) ptr[0] << 24) |
           ((uint32_t) ptr[1] << 16) |
           ((uint32_t) ptr[2] << 8) | 
           ((uint32_t) ptr[3]);
}


static inline
uint64_t read_be64(const uint8_t *ptr)
{
    return ((uint64_t) ptr[0] << 56) |
           ((uint64_t) ptr[1] << 48) |
           ((uint64_t) ptr[2] << 40) |
           ((uint64_t) ptr[3] << 32) |
           ((uint64_t) ptr[4] << 24) |
           ((uint64_t) ptr[5] << 16) |
           ((uint64_t) ptr[6] << 8) | 
           ((uint64_t) ptr[7]);
}

#ifdef __cplusplus
}
#endif
#endif
