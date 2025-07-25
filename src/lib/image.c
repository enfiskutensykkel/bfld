#include "image.h"
#include <utils/rbtree.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>


int image_create(struct image **image)
{
    *image = NULL;

    //long pgsz = sysconf(_SC_PAGESIZE);

    struct image *img = malloc(sizeof(struct image));
    if (img == NULL) {
        return ENOMEM;
    }

    rb_tree_init(&img->symbols);

    *image = img;
    return 0;
}


void image_destroy(struct image **image)
{
    if (*image != NULL) {
        free(*image);
        *image = NULL;
    }
}
