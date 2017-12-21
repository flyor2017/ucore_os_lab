#ifndef __BOOT_ASM_H__
#define __BOOT_ASM_H__

/* Assembler macros to create x86 segments */

/* Normal segment */
#define SEG_NULLASM                                             \
    .word 0, 0;                                                 \
    .byte 0, 0, 0, 0

/*
段描述符格式：

|   7    |     6       |     5     |   4    |   3    |   2    |   1    |   0    |  字节
|76543210|7 6 5 4 3210 |7 65 4 3210|76543210|76543210|76543210|76543210|76543210|  比特
|--------|-|-|-|-|---- |-|--|-|----|--------|--------|--------|--------|--------|  占位
|  BASE  |G|D|0|A|LIMIT|P|D |S|TYPE|<------- BASE 23-0 ------>|<-- LIMIT 15-0 ->|  含义
|  31-24 | |/| |V|19-16| |P |
           |B| |L|     | |L |

BASE: 段基址，由上图中的两部分(BASE 31-24 和 BSE23-0)组成
G：LIMIT的单位，该位 0 表示单位是字节，1表示单位是 4KB
D/B: 该位为 0 表示这是一个 16 位的段，1 表示这是一个 32 位段
AVL: 该位是用户位，可以被用户自由使用
LIMIT: 段的界限，单位由 G 位决定。数值上（经过单位换算后的值）等于段的长度（字节）- 1。
P: 段存在位，该位为 0 表示该段不存在，为 1 表示存在。
DPL：段权限
S: 该位为 1 表示这是一个数据段或者代码段。为 0 表示这是一个系统段（比如调用门，中断门等）
TYPE: 根据 S 位的结果，再次对段类型进行细分。
*/	

// 下面的 0x90 已经默认 该段位存在且这是一个数据或代码段，段权限为0
// 下面的 0xc0 已经默认 该段位的limit是以4k为单位且是一个32位段
// 下面默认传过来的是一个32位的lim， 且用的是其高20位。
#define SEG_ASM(type,base,lim)                                  \
    .word (((lim) >> 12) & 0xffff), ((base) & 0xffff);          \
    .byte (((base) >> 16) & 0xff), (0x90 | (type)),             \
        (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)


/* Application segment type bits */
#define STA_X       0x8     // Executable segment
#define STA_E       0x4     // Expand down (non-executable segments)
#define STA_C       0x4     // Conforming code segment (executable only)
#define STA_W       0x2     // Writeable (non-executable segments)
#define STA_R       0x2     // Readable (executable segments)
#define STA_A       0x1     // Accessed

#endif /* !__BOOT_ASM_H__ */

