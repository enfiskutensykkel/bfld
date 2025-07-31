#ifndef __BFLD_X86_64_H__
#define __BFLD_X86_64_H__

#define RELOC_X86_64_NONE   0x00
#define RELOC_X86_64_ABS64  0x01
#define RELOC_X86_64_PC32   0x02
#define RELOC_X86_64_ABS32  0x10
#define RELOC_X86_64_ABS32S 0x11


extern const struct arch_handler x86_64_handler;

#endif
