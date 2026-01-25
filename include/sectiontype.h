#ifndef BFLD_SECTION_TYPE_H
#define BFLD_SECTION_TYPE_H

/*
 * Section types.
 */
enum section_type
{
    SECTION_ZERO,       // section without contents, i.e., uninitialized variables (.bss, .common)
    SECTION_DATA,       // section with data contents, for example variables (.data)
    SECTION_RODATA,     // section contains read-only data, for example strings (.rodata)
    SECTION_TEXT,       // section contains machine code (.text)
    SECTION_MAX_TYPES
};

#endif
