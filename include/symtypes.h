#ifndef __BFLD_SYMBOL_TYPES_H__
#define __BFLD_SYMBOL_TYPES_H__
#ifdef __cplusplus
extern "C" {
#endif


/*
 * Symbol binding types.
 *
 * The binding type determines what to do if multiple symbols declarations
 * with the same name exists.
 *
 * SYMBOL_GLOBAL: The symbol is considered strong and takes presedence.
 * SYMBOL_LOCAL:  The symbol is considered strong and takes presedence.
 * SYMBOL_WEAK:   The symbol is considered weak and can be overridden by
 *                later declarations with the same name.
 *
 * Local means that the symbol is only visible to the specific object file
 * where it is defined, whereas global (and weak) means that the symbol is
 * global across multiple files.
 */
enum symbol_binding
{
    SYMBOL_LOCAL,
    SYMBOL_GLOBAL,
    SYMBOL_WEAK
};


enum symbol_type
{
    SYMBOL_NOTYPE,
    SYMBOL_DATA,
    SYMBOL_FUNCTION
};


#ifdef __cplusplus
}
#endif
#endif
