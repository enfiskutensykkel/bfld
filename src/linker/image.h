#ifndef __BFLD_IMAGE_H__
#define __BFLD_IMAGE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <utils/list.h>


/* 
 * Convenience macro for aligning addresses to a specified alignment.
 * Alignment must be a power of two.
 */
#define ALIGN_ADDR(size, alignment) \
    (((uint64_t) (size) + (alignment) - 1) & ~(((uint64_t) (alignment) - 1)))


enum segment_type
{
    SEG_TYPE_NULL,
    SEG_TYPE_CODE,
    SEG_TYPE_DATA
};


struct segment
{
    enum segment_type type;
    uint64_t addr;
    size_t size;
    struct list_head listh;
};


struct image
{
    size_t filesz;  // Total file size of image
};


int create_image(struct image **image);


void destroy_image(struct image **image);



#ifdef __cplusplus
}
#endif
#endif
