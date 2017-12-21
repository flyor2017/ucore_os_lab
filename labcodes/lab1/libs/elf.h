#ifndef __LIBS_ELF_H__
#define __LIBS_ELF_H__

#include <defs.h>

#define ELF_MAGIC    0x464C457FU            // "\x7FELF" in little endian

/* file header */
// 这里的ELF文件头占52个字节
struct elfhdr {
    uint32_t e_magic;     // must equal ELF_MAGIC
    uint8_t e_elf[12];
    
	uint16_t e_type;      // 1=relocatable, 2=executable, 3=shared object, 4=core image
    uint16_t e_machine;   // 3=x86, 4=68K, etc.
    
	uint32_t e_version;   // file version, always 1
    uint32_t e_entry;     // entry point if executable
    uint32_t e_phoff;     // file position of program header or 0
    uint32_t e_shoff;     // file position of section header or 0
    uint32_t e_flags;     // architecture-specific flags, usually 0
    
	uint16_t e_ehsize;    // size of this elf header
    uint16_t e_phentsize; // size of an entry in program header
    uint16_t e_phnum;     // number of entries in program header or 0
    uint16_t e_shentsize; // size of an entry in section header
    uint16_t e_shnum;     // number of entries in section header or 0
    uint16_t e_shstrndx;  // section number that contains section name strings
};

/*
#define EI_NIDENT 16
typedef struct{
　　unsigned char e_ident[EI_NIDENT];
　　Elf32_Half e_type;
　　Elf32_Half e_machine;
　　Elf32_Word e_version;
　　Elf32_Addr e_entry;
　　Elf32_Off e_phoff;
　　Elf32_Off e_shoff;
　　Elf32_Word e_flags;
　　Elf32_Half e_ehsize;
　　Elf32_Half e_phentsize;
　　Elf32_Half e_phnum;
　　Elf32_Half e_shentsize;
　　Elf32_Half e_shnum;
　　Elf32_Half e_shstrndx;
}Elf32_Ehdr;

最开头是16个字节的e_ident, 其中包含用以表示ELF文件的字符，以及其他一些与机器无关的信息。
开头的4个字节值固定不变，为0x7f和ELF三个字符。
e_type 它标识的是该文件的类型。
e_machine 表明运行该程序需要的体系结构。
e_version 表示文件的版本。
e_entry 程序的入口地址。
e_phoff 表示Program header table 在文件中的偏移量（以字节计数）。
e_shoff 表示Section header table 在文件中的偏移量（以字节计数）。
e_flags 对IA32而言，此项为0。
e_ehsize 表示ELF header大小（以字节计数）。
e_phentsize 表示Program header table中每一个条目的大小。
e_phnum 表示Program header table中有多少个条目。
e_shentsize 表示Section header table中的每一个条目的大小。
e_shnum 表示Section header table中有多少个条目。
e_shstrndx 包含节名称的字符串是第几个节（从零开始计数）。

*/

/* program section header */
struct proghdr {
    uint32_t p_type;   // loadable code or data, dynamic linking info,etc.
    uint32_t p_offset; // file offset of segment
    uint32_t p_va;     // virtual address to map segment
    uint32_t p_pa;     // physical address, not used
    uint32_t p_filesz; // size of segment in file
    uint32_t p_memsz;  // size of segment in memory (bigger if contains bss）
    uint32_t p_flags;  // read/write/execute bits
    uint32_t p_align;  // required alignment, invariably hardware page size
};

#endif /* !__LIBS_ELF_H__ */

