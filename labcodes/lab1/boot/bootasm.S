#include <asm.h>

# Start the CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.
# 这段代码应该放在MBR的前446个字节处，BIOS将会把这段
# 代码加载到内存0x7c00处，并跳转到该处执行此代码。
# 这段代码将完成实模式到保护模式的转换。


# .set命令类似于c语言中的define，可以为一个符号指定一个常量表达式。
.set PROT_MODE_CSEG,        0x8                     # kernel code segment selector
.set PROT_MODE_DSEG,        0x10                    # kernel data segment selector
.set CR0_PE_ON,             0x1                     # protected mode enable flag

# start address should be 0:7c00, in real mode, the beginning address of the running bootloader
# .globl或.global 用于定义全局标号
.globl start
start:

#16位汇编
.code16                                             # Assemble for 16-bit mode

	#关闭中断
    cli                                             # Disable interrupts
	
	#cld和std是用于指定字符串传送命令movsb或movsw的传送方向的
	#其中cld使DF复位，即DF=0，传送时偏移递增，std使DF置位，即DF=1，传送时偏移递减。
	#MOVSB 字节串传送 DF=0, SI = SI + 1 , DI = DI + 1 ；DF = 1 , SI = SI - 1 , DI = DI - 1
	#MOVSW 字串传送 DF=0, SI = SI + 2 , DI = DI + 2 ；DF = 1 , SI = SI - 2 , DI = DI - 2
    cld                                             # String operations increment

    # Set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    movw %ax, %ds                                   # -> Data Segment
    movw %ax, %es                                   # -> Extra Segment
    movw %ax, %ss                                   # -> Stack Segment

	/*
	A20 地址线问题
		1981 年8 月，IBM 公司最初推出的个人计算机IBM PC 使用的CPU 是Intel 8088。
		在该微机中地址线只有20 根(A0 – A19)。在当时内存RAM 只有几百KB 或不到1MB 时，
		20 根地址线已足够用来寻址这些内存。其所能寻址的最高地址是0xffff:0xffff，也即0x10ffef。
		对于超出0x100000(1MB)的寻址地址将默认地环绕到0x0ffef。当IBM 公司于1985 年引入AT 机时，
		使用的是Intel 80286 CPU，具有24 根地址线,最高可寻址16MB，并且有一个与8088 完全兼容的
		实模式运行方式。然而，在寻址值超过1MB 时它却不能象8088 那样实现地址寻址的环绕。
		但是当时已经有一些程序是利用这种地址环绕机制进行工作的。为了实现完全的兼容性，
		IBM 公司发明了使用一个开关来开启或禁止0x100000 地址比特位。由于在当时的8042键盘
		控制器上恰好有空闲的端口引脚（输出端口P2，引脚P21），于是便使用了该引脚来作为与门控制
		这个地址比特位。该信号即被称为A20。如果它为零，则比特20 及以上地址都被清除。
		从而实现了兼容性。由于在机器启动时，默认条件下，A20 地址线是禁止的，所以操作系统
		必须使用适当的方法来开启它。但是由于各种兼容机所使用的芯片集不同，要做到这一点却是
		非常的麻烦。因此通常要在几种控制方法中选择。对A20 信号线进行控制的常用方法是通过
		设置键盘控制器的端口值。对于其它一些兼容微机还可以使用其它方式来做到对A20 线的控制。
		有些操作系统将A20 的开启和禁止作为实模式与保护运行模式之间进行转换的标准过程中的一部分。
		由于键盘的控制器速度很慢，因此就不能使用键盘控制器对A20 线来进行操作。为此引进了一个
		A20 快速门选项(Fast Gate A20)，它使用I/O 端口0x92 来处理A20 信号线，避免了使用慢速
		的键盘控制器操作方式。对于不含键盘控制器的系统就只能使用0x92 端口来控制，但是该端口
		也有可能被其它兼容微机上的设备（如显示芯片）所使用，从而造成系统错误的操作。
		还有一种方式是通过读0xee 端口来开启A20 信号线，写该端口则会禁止A20 信号线。
	*/
    # Enable A20:
    #  For backwards compatibility with the earliest PCs, physical
    #  address line 20 is tied low, so that addresses higher than
    #  1MB wrap around to zero by default. This code undoes this.
seta20.1:
	
	# inb 从I/O端口读取一个字节(BYTE, HALF-WORD) ;
	# outb 向I/O端口写入一个字节（BYTE, HALF-WORD） ;
	# inw 从I/O端口读取一个字（WORD，即两个字节） ;
	# outw 向I/O端口写入一个字（WORD，即两个字节） ;
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    
	# Test对两个参数(目标，源)执行AND逻辑操作,并根据结果设置标志寄存器,结果本身不会保存。
	# 如果目标数和源的AND操作的结果是0，则将ZF标志位置位1，否则将ZF标志位置位0.
	testb $0x2, %al
    jnz seta20.1

    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64                                 # 0xd1 means: write data to 8042's P2 port

seta20.2:
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.2

    movb $0xdf, %al                                 # 0xdf -> port 0x60
    outb %al, $0x60                                 # 0xdf = 11011111, means set P2's A20 bit(the 1 bit) to 1

    # Switch from real to protected mode, using a bootstrap GDT
    # and segment translation that makes virtual addresses
    # identical to physical addresses, so that the
    # effective memory map does not change during the switch.
	#加载全局描述符表
    lgdt gdtdesc
	#将cr0寄存器的第0位置位1，表示开启保护模式。
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0

    # Jump to next instruction, but in 32-bit code segment.
    # Switches processor into 32-bit mode.
	# 由于上面的cr0的第0位已经置位，进入了保护模式，所以这里不管是用16位的指令还是32位的指令
	# 都是表示调到 0x0008 这个段选择子选中的GDT表中的那一项。
	# 由于段选择子的格式为 xxxxxxxxxxxxx x xx,也就是最低两位为运行的特权级别
	# 第2位为运行的段基址是在GDT中还是在LDT中，所以这里的 0x0008 实际上表示
	# 在GDT中的第一项，且以0特权级别运行。
    ljmp $PROT_MODE_CSEG, $protcseg

#32位汇编
.code32                                             # Assemble for 32-bit mode
protcseg:
    # Set up the protected-mode data segment registers
	# 设置ds、es、fs、gs、ss这些段选择子指向GDT中的第二项。
    movw $PROT_MODE_DSEG, %ax                       # Our data segment selector
    movw %ax, %ds                                   # -> DS: Data Segment
    movw %ax, %es                                   # -> ES: Extra Segment
    movw %ax, %fs                                   # -> FS
    movw %ax, %gs                                   # -> GS
    movw %ax, %ss                                   # -> SS: Stack Segment

    # Set up the stack pointer and call into C. The stack region is from 0--start(0x7c00)
    
	movl $0x0, %ebp
    movl $start, %esp
    call bootmain

    # If bootmain returns (it shouldn't), loop.
spin:
    jmp spin

# Bootstrap GDT

# .p2align n 表示地址对齐，其地址必须是 2^n 的正数倍。
.p2align 2                                          # force 4 byte alignment
gdt:
    SEG_NULLASM                                     # null seg
    SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)           # code seg for bootloader and kernel
    SEG_ASM(STA_W, 0x0, 0xffffffff)                 # data seg for bootloader and kernel

# 将被读取到GDTR中的值，GDTR表的格式如下：
# 32位基地址 | 16位表的limit
gdtdesc:
    .word 0x17                                      # sizeof(gdt) - 1
    .long gdt                                       # address gdt
