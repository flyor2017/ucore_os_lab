#ifndef __LIBS_X86_H__
#define __LIBS_X86_H__

#include <defs.h>

/*

下面这个宏完成 n = n / base; 除法，支持 n 为64位被除数，保证商也是64位数

默认32位系统下汇编最高只支持64位被除数，32位除数，32位商，32位余数。
但是这对于大数除以极小的整数会出问题，比如 0x1234567890/1 这个商32位的寄存器（内存单元）存不下。

寄存器限定符A表示同时限定寄存器eax和edx。其中操作数的高32位放在edx中，低32位放在eax中

*/
#define do_div(n, base) ({                                        \
    unsigned long __upper, __low, __high, __mod, __base;        \
    __base = (base);                                            \
    asm("" : "=a" (__low), "=d" (__high) : "A" (n));            \
    __upper = __high;                                            \
    if (__high != 0) {                                            \
        __upper = __high % __base;                                \
        __high = __high / __base;                                \
    }                                                            \
    asm("divl %2" : "=a" (__low), "=d" (__mod)                    \
        : "rm" (__base), "0" (__low), "1" (__upper));            \
    asm("" : "=A" (n) : "a" (__low), "d" (__high));                \
    __mod;                                                        \
 })

static inline uint8_t inb(uint16_t port) __attribute__((always_inline));
static inline void insl(uint32_t port, void *addr, int cnt) __attribute__((always_inline));
static inline void outb(uint16_t port, uint8_t data) __attribute__((always_inline));
static inline void outw(uint16_t port, uint16_t data) __attribute__((always_inline));
static inline uint32_t read_ebp(void) __attribute__((always_inline));

/* Pseudo-descriptors used for LGDT, LLDT(not used) and LIDT instructions. */
//定义一个LGDT或LIDT项，用__attribute__((packed))保证其不会因为字节对齐而引发的错误。
//这里需要注意的是，由于结构体的存储是由低地址向高地址，所以要先写limit再写base
//真实的GDTR、LDTR和IDTR存储时其低16位为limit，高32位为base。
struct pseudodesc {
    uint16_t pd_lim;        // Limit
    uint32_t pd_base;        // Base address
} __attribute__ ((packed));

static inline void lidt(struct pseudodesc *pd) __attribute__((always_inline));
static inline void sti(void) __attribute__((always_inline));
static inline void cli(void) __attribute__((always_inline));
static inline void ltr(uint16_t sel) __attribute__((always_inline));

static inline uint8_t
inb(uint16_t port) {
    uint8_t data;
	//从port端口读取一个字节的数据到data内存中。
    asm volatile ("inb %1, %0" : "=a" (data) : "d" (port));
    return data;
}

static inline void
insl(uint32_t port, void *addr, int cnt) {
	
	//cld表示清楚DF标志位，即DF=0，这时传送方向为增项传送
	// Repeat Prefix	Termination Condition 1		Termination Condition 2
	// REP				ECX=0						None
	// REPE/REPZ		ECX=0						ZF=0
	// REPNE/REPNZ		ECX=0						ZF=1
	//ins是将端口（由DX指出）中的字符串传送到指定内存（ES:EDI）处。
	//所以下面这段代码的作用的从port指定的端口中读取cnt*4个字节的数据到addr指向的内存中
    asm volatile (
            "cld;"
            "repne; insl;"
            : "=D" (addr), "=c" (cnt)
            : "d" (port), "0" (addr), "1" (cnt)
            : "memory", "cc");
}

static inline void
outb(uint16_t port, uint8_t data) {
	//向端口port写入一个字节的数据，内容为data指向的内容。
    asm volatile ("outb %0, %1" :: "a" (data), "d" (port));
}

static inline void
outw(uint16_t port, uint16_t data) {
	//向port指定的端口输出一个字（两个字节），内容为data指向的内容。
    asm volatile ("outw %0, %1" :: "a" (data), "d" (port));
}

static inline uint32_t
read_ebp(void) {
    uint32_t ebp;
	//读取寄存器ebp中的内容
    asm volatile ("movl %%ebp, %0" : "=r" (ebp));
    return ebp;
}

static inline void
lidt(struct pseudodesc *pd) {
	//将pd指向的内存地址值赋值给IDTR，届时即表示已经加载了终端描述符表
    asm volatile ("lidt (%0)" :: "r" (pd));
}

static inline void
sti(void) {
	//开中断
    asm volatile ("sti");
}

static inline void
cli(void) {
	// 关中断
    asm volatile ("cli");
}

static inline void
ltr(uint16_t sel) {
	//ltr 加载(load)任务(task)寄存器(register)
    asm volatile ("ltr %0" :: "r" (sel));
}

static inline int __strcmp(const char *s1, const char *s2) __attribute__((always_inline));
static inline char *__strcpy(char *dst, const char *src) __attribute__((always_inline));
static inline void *__memset(void *s, char c, size_t n) __attribute__((always_inline));
static inline void *__memmove(void *dst, const void *src, size_t n) __attribute__((always_inline));
static inline void *__memcpy(void *dst, const void *src, size_t n) __attribute__((always_inline));

#ifndef __HAVE_ARCH_STRCMP
#define __HAVE_ARCH_STRCMP
static inline int
__strcmp(const char *s1, const char *s2) {
    int d0, d1, ret;
	/*
	字符串传送指令
		lods{bwl}指令[load string]，将esi指向的地址处的数据取出来赋给AL/AX/EAX寄存器;
		stos{bwl}指令[store string]，将AL/AX/EAX寄存器的值取出来赋给edi所指向的地址处。

	字符比较指令
		scas{bwl}指令[scan string]，
		scas指令是用al(或ax)中的值对目的串(es:di或edi)中的字节(或字)进行扫描,常与repnz(不相等继续)或repz(相等继续)连用,
		cmps是用源串(si或esi)与目的串(es:di或edi)进行字节或字比较,也常与repz(相等继续)或repnz(不相等继续)连用,
	
	关于标号
		标号可以定义在汇编语言程序的任意位置，被定义的标号将代表标号所在位置的地址值
		标号分为两种：符号标号和数字（0-9单个数字）标号
		符号标号默认具有全局（global）标识，且只能被定义一次，但是以 . 开头的标号被定义为只有本地（local）作用域。
		数字标号可以被定义多次，通过加后缀b（before）和f（forward）来区分。
		1:	#定义数字标号1
		one:	#定义符号标号one
		jmp 1f	# 1f 表示在当前语言后面（forward），且里当前语句最近的为 1 的数字标号，等价于 jmp two
		jmp 1b	# 1b 表示在当前语言前面（before），且里当前语句最近的为 1 的数字标号，等价于 jmp one
		
		1:	#重定义数字标号1
		two:	#定义符号标号two
		jmp 1b	# 1b 表示在当前语言前面（before），且里当前语句最近的为 1 的数字标号，等价于 jmp two
		
	逻辑运算
		xor：异或
		or：或
		
	算术运算
		sbb src des: 借位减法（subtract with borrow）方向：des = des - src
	
	下面指令的功能：
		do{
			al = (ds:esi);
			if(al == (es:edi)){	//注意这里还会对CF为产生影响，如果(es:edi)-al产生借位，则CF=1，否则CF=0
				if((al) == 0){
					eax = (eax) ^ (eax);	// eax = 0;
					break;
				}else{
					continue;
				}
			}else{
				eax = (eax) - (eax) - (CF);	//eax = -(CF);
				al = (al) | 0x01;
			}
		}while(true);
		
	加强版（将会返回第一个不想等的字符的差值【源-目标】，比如源串 "hel5" 和目标串 "hell"，将返回 '5' - 'l' = -55）
		asm volatile (
             "cld;"	//设置字符串为增项
             "1: lodsb;"
             "scasb;"
             "jne 2f;"
             "testb %%al, %%al;"
             "jne 1b;"
             "xorl %%eax, %%eax;"
             "jmp 3f;"
             "2: subb %%es:-1(%%rdi), %%al;"
             "movsbl %%al, %%eax;"	//符号扩展
             "3:"
             : "=a" (ret), "=&S" (d0), "=&D" (d1)
             : "1" (s1), "2" (s2)
             : "memory");
	*/
    asm volatile (
            "1: lodsb;"
            "scasb;"
            "jne 2f;"
            "testb %%al, %%al;"
            "jne 1b;"
            "xorl %%eax, %%eax;"
            "jmp 3f;"
            "2: sbbl %%eax, %%eax;"
            "orb $1, %%al;"
            "3:"
            : "=a" (ret), "=&S" (d0), "=&D" (d1)
            : "1" (s1), "2" (s2)
            : "memory");
    return ret;
}

#endif /* __HAVE_ARCH_STRCMP */

#ifndef __HAVE_ARCH_STRCPY
#define __HAVE_ARCH_STRCPY
static inline char *
__strcpy(char *dst, const char *src) {
    int d0, d1, d2;
	/*
	
	下面指令功能：
		do{
			al = (ds:esi);
			(es:edi) = al;
		}while(al & al);	// al != 0
	
	关于constraint修饰符：
		= 表示给定的操作数是一个只能用于写操作的操作数。
		+ 表示给定操作数可读可写
		& 表示给定的操作数与某一输入操作数不能使用同一个寄存器，如果输入操作数是由序号给出则可以是同一个操作数。
			eg1：
				__asm__("popl %0; movl %1,%%esi; movl %2,%%edi;":"=&a"(__out):"a"(__in1),"c"(__in2));
				//编译出错，输入操作数__in1与输出操作数__out共用同一个寄存器al(ax/eax/rax)。
			eg2:
				__asm__("popl %0; movl %1,%%esi; movl %2,%%edi;":"=&a"(__out):"0"(__in1),"c"(__in2));
				//编译成功，虽然这里输入操作数__in1和输出操作数__out也是共用同一个寄存器al(ax/eax/rax),
				//但是输入操作数是由序号给出，故可以通过。
	*/
    asm volatile (
            "1: lodsb;"
            "stosb;"
            "testb %%al, %%al;"
            "jne 1b;"
            : "=&S" (d0), "=&D" (d1), "=&a" (d2)
            : "0" (src), "1" (dst) : "memory");
    return dst;
}
#endif /* __HAVE_ARCH_STRCPY */

#ifndef __HAVE_ARCH_MEMSET
#define __HAVE_ARCH_MEMSET
static inline void *
__memset(void *s, char c, size_t n) {
    int d0, d1;
	
	/*
	
	下面指令功能：
		while(%ecx--){
			(%es:%edi) = al;
		}
		
	注意：rep指令是while循环而不是do-while循环。也就是说当eax一开始就是0时，其后一条指令会被跳过，并且不再循环。
	*/
	
    asm volatile (
            "rep; stosb;"
            : "=&c" (d0), "=&D" (d1)
            : "0" (n), "a" (c), "1" (s)
            : "memory");
    return s;
}
#endif /* __HAVE_ARCH_MEMSET */

#ifndef __HAVE_ARCH_MEMMOVE
#define __HAVE_ARCH_MEMMOVE
static inline void *
__memmove(void *dst, const void *src, size_t n) {
	/*
	memcpy和memmove的区别在于处理内存重叠上，当两块给定内存出现重叠时，memcpy不能保证结果正确，而memmove可以。
	*/
    
	
	if (dst < src) {//不会出现内存重叠
        return __memcpy(dst, src, n);
    }
	
	//可能出现内存重叠
    int d0, d1, d2;
    asm volatile (
            "std;"
            "rep; movsb;"
            "cld;"
            : "=&c" (d0), "=&S" (d1), "=&D" (d2)
            : "0" (n), "1" (n - 1 + src), "2" (n - 1 + dst)
            : "memory");
    return dst;
}
#endif /* __HAVE_ARCH_MEMMOVE */

#ifndef __HAVE_ARCH_MEMCPY
#define __HAVE_ARCH_MEMCPY
static inline void *
__memcpy(void *dst, const void *src, size_t n) {
    int d0, d1, d2;
	
	/*
	
	下面的代码首先计算出要复制的的双字（4字节）数，以双字方式进行移动（加快移动速度）。
	待所有要移双字移完以后判断剩下的要移动的字节数（不够组成一个双字）。
	
	shr(shift right) ：逻辑右移指令
	sar(shift arithmatic right) : 算术右移指令
	*/
    asm volatile (
			"shrl 2, %%ecx;"
            "rep; movsl;"
            "movl %4, %%ecx;"
            "andl $3, %%ecx;"
            "jz 1f;"
            "rep; movsb;"
            "1:"
            : "=&c" (d0), "=&D" (d1), "=&S" (d2)
            : "0" (n), "g" (n), "1" (dst), "2" (src)
            : "memory");
    return dst;
}
#endif /* __HAVE_ARCH_MEMCPY */

#endif /* !__LIBS_X86_H__ */

