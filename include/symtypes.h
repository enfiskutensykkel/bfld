#ifndef __BFLD_SYMBOL_TYPES_H__
#define __BFLD_SYMBOL_TYPES_H__
#ifdef __cplusplus
extern "C" {
#endif


enum symbol_type
{
    SYMBOL_NOTYPE,  // symbol has no data/definition
    SYMBOL_OBJECT,  // symbol contains a data object
    SYMBOL_TLS,     // symbol contains a thread-local data object (thread-local storage)
    SYMBOL_FUNCTION // symbol is a function
};


/* Forward declarations */
struct symbol;
struct symref;
struct symtab;


#ifdef __cplusplus
}
#endif
#endif
