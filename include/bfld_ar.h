#ifndef __BFLD_AR_H__
#define __BFLD_AR_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "bfld_list.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>


struct bfld_archive_member
{
    struct bfld_list list_entry;
    size_t size;
    uint8_t data[];
};


struct bfld_archive
{
    struct bfld_list members;
};


int bfld_read_archive(FILE *fp, struct bfld_archive **archive);


void bfld_free_archive(struct bfld_archive **archive);

#ifdef __cplusplus
}
#endif
#endif
