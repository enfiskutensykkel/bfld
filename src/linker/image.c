#include "image.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>


int create_image(struct image **image)
{
    *image = NULL;

    long pgsz = sysconf(_SC_PAGESIZE);

    struct image *img = malloc(sizeof(struct image));
    if (img == NULL) {
        return ENOMEM;
    }

    img->filesz = 0;

    *image = img;
    return 0;
}


void destroy_image(struct image **image)
{
    if (*image != NULL) {
        free(*image);
        *image = NULL;
    }
}
