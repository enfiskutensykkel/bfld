#ifndef __BFLD_SECTION_TYPES_H__
#define __BFLD_SECTION_TYPES_H__
#ifdef __cplusplus
extern "C" {
#endif

enum section_type
{
    SECTION_ZERO,   // Section without contents, i.e., unitialized variables (.bss, .common)
    SECTION_DATA,   // Section with data contents, for example variables. (.data)
    SECTION_RODATA, // Section contains read-only data, for example strings (.rodata)
    SECTION_TEXT    // Section contains machine code
};


/* Forward declaration of section */
struct section;


#ifdef __cplusplus
}
#endif
#endif
