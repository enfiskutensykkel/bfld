#ifndef __BFLD_IMAGE_H__
#define __BFLD_IMAGE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <utils/rbtree.h>
#include <utils/list.h>
#include <stddef.h>
#include <stdint.h>


/*
 * Intermediate representation of an "image"; 
 * what will eventually be written to disk
 * as a complete executable.
 */
struct image
{
    struct rb_tree symbols; // A map of all symbols
};


int image_create(struct image **img);


void image_destroy(struct image **image);


#ifdef __cplusplus
}
#endif
#endif


//enum segment_type
//{
//    SEG_TYPE_NULL,
//    SEG_TYPE_CODE,
//    SEG_TYPE_DATA
//};
//
//
//struct segment
//{
//    enum segment_type type;
//    uint64_t addr;
//    size_t size;
//    struct list_head listh;
//};
//
//
///*
// * File image rep
// */
//struct image
//{
//};



