#ifndef __BFLD_VM_H__
#define __BFLD_VM_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "bfld_list.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>


enum bfld_bytecode_type
{
    BFLD_BYTECODE_DATA,
    BFLD_BYTECODE_TEXT,
    BFLD_BYTECODE_CODE,
    BFLD_BYTECODE_CONST
};


struct bfld_vm_bytecode
{
    uint16_t idx;
    struct list_head list_entry;
    enum bfld_bytecode_type type;
    uint64_t addr;
    size_t size;
    uint8_t bytecode[];
};


struct bfld_vm_sym
{
    struct list_head list_entry;
    uint16_t bytecode_idx;
    size_t len;
    char name[];
};


struct bfld_vm
{
    struct list_head syms;
    struct list_head bytecode;
};


int bfld_vm_load(FILE *fp, struct bfld_vm **vm);


void bfld_vm_free(struct bfld_vm **vm);


#ifdef __cplusplus
}
#endif
#endif
