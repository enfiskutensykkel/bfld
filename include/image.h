#ifndef __BFLD_IMAGE_H__
#define __BFLD_IMAGE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <utils/list.h>
#include <stddef.h>
#include <stdint.h>


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


/*
 * File image rep
 */
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
