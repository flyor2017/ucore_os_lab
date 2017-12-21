#include <defs.h>
#include <x86.h>
#include <elf.h>

/* *********************************************************************
 * This a dirt simple boot loader, whose sole job is to boot
 * an ELF kernel image from the first IDE hard disk.
 *
 * DISK LAYOUT
 *  * This program(bootasm.S and bootmain.c) is the bootloader.
 *    It should be stored in the first sector of the disk.
 *
 *  * The 2nd sector onward holds the kernel image.
 *
 *  * The kernel image must be in ELF format.
 *
 * BOOT UP STEPS
 *  * when the CPU boots it loads the BIOS into memory and executes it
 *
 *  * the BIOS intializes devices, sets of the interrupt routines, and
 *    reads the first sector of the boot device(e.g., hard-drive)
 *    into memory and jumps to it.
 *
 *  * Assuming this boot loader is stored in the first sector of the
 *    hard-drive, this code takes over...
 *
 *  * control starts in bootasm.S -- which sets up protected mode,
 *    and a stack so C code then run, then calls bootmain()
 *
 *  * bootmain() in this file takes over, reads in the kernel and jumps to it.
 * */

#define SECTSIZE        512
#define ELFHDR          ((struct elfhdr *)0x10000)      // scratch space

/*
I/O寻址空间说明：
端口地址范围	分配说明					|	端口地址范围	配说明
--------------------------------------------|---------------------------------------------
0x000～0x01F	8237A DMA控制器1			|	0x1F0～0x1F7	IDE硬盘控制器0
0x020～0x03F	8259A 可编程中断控制器1		|	0x278～0x27F	并行打印机端口2
0x040～0x05F	8253/8254A 定时计数器		|	0x2F8～0x2FF	串行控制器2
0x060～0x06F	8042 键盘控制器				|	0x378～0x37F	并行打印机端口1
0x070～0x07F	访问CMOS RAM/实时时钟RTC（Real Time Clock）端口 |	0x3B0～0x3BF	单色MDA显示控制器
0x080～0x09F	DMA页面寄存器访问端口		|	0x3C0～0x3CF	彩色CGA显示控制器
0x0A0～0x0BF	8259A 可编程中断控制器2		|	0x3D0～0x3DF	彩色EGA/VGA显示控制器
0x0C0～0x0DF	8237A DMA控制器2			|	0x3F0～0x3F7	软盘控制器
0x0F0～0x0FF	协处理器访问端口			|	0x3F8～0x3FF	串行控制器1
0x170～0x177	IDE硬盘控制器1

*/
/* waitdisk - wait for disk ready */
static void
waitdisk(void) {
	
/*
	1F7端口为主状态寄存器（读）/命令寄存器（写）
	在读取数据时，其格式如下：
	0x01		0x02		0x04		0x08		0x10		0x20		0x40		0x80
	ERR_STAT	INDEX_STAT	ECC_STAT	DRQ_STAT	SEEK_STAT	WRERR_STAT	READY_STAT	BUSY_STAT
	
	ERR_STAT:前一条命令执行错误。
	INDEX_STAT:收到索引，当磁盘旋转遇到索引标志时会置位
	ECC_STAT:ECC校验出错，遇到一个可恢复错误并且已经纠正时会置位
	DRQ_STAT:数据请求服务，当数据准备好可以向主机传送时会置位
	SEEK_STAT:寻道结束，当寻道完成，磁头停在指定磁道上会置位
	WRERR_STAT:驱动器故障（写错误），当发生错误时该位不会改变，只有主机读取寄存器状态后，该位才会再次表现当前的出错状态。
	READY_STAT:驱动器准备好，表示驱动器准备好可以接收命令。
	BUSY_STAT:控制器忙碌
*/
    while ((inb(0x1F7) & 0xC0) != 0x40)
        /* do nothing */;
}

/* readsect - read a single sector at @secno into @dst */
//这个函数采用的是程序查询方式进行数据读取
static void
readsect(void *dst, uint32_t secno) {
    // wait for disk to be ready
    waitdisk();

	/*
	IDE硬盘控制器0的相关端口：
	0x1F0	HD_DATA		数据寄存器
		CPU通过该寄存器向磁盘读取或写入一个扇区的数据，
		使用命令rep outsw或rep insw重复读写ecx=256个字。
	0x1F1	HD_ERROR	错误寄存器
		该寄存器有8位错误状态，但是只有主状态寄存器的第0位为1时，该寄存器才有效。
	0x1F2	HD_NSECTOR	扇区数寄存器
		该寄存器存储当前操作指令操作的扇区数，每完成一个扇区数该寄存器自动减1
		直到为0。若初值为0则表示传输的最大扇区数256。
	0x1F3	HD_SECTOR	扇区号/LBA0-7位寄存器
		该寄存器存取当前操作的起始扇区号，每当完成一个扇区的操作后该寄存器会自增1.
	0x1F4	HD_LCYL		柱面号/LBA8-15位寄存器（低）
	0x1F5	HD_HCYL		柱面号/LBA16-23位寄存器（高）
		两个柱面号寄存器分别存放柱面号的高2位和低8位。
	0x1F6	HD_CURRENT	磁头/LBA24-27位寄存器
		该寄存器存放操作指令操作的当前的驱动器和磁头号，其格式为1L1Dhhhh。
		L表示寻址方式(0CHS寻址或1LBA寻址)，D表示选择驱动器（0主驱动或1从驱动），
		hhhh表示选择的磁头或者逻辑地址的第24-27位。
	0x1F7	HD_STATUS	主状态寄存器（读）/命令寄存器（写）
	
	*/
	
	//这里采用LBA寻址方式每次读取硬盘上的一个扇区的内容
    outb(0x1F2, 1);                         // count = 1
    outb(0x1F3, secno & 0xFF);
    outb(0x1F4, (secno >> 8) & 0xFF);
    outb(0x1F5, (secno >> 16) & 0xFF);
    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
	
	/*
	常见的命令：20H，30H，90H
	20H	读扇区命令(带重试)：从硬盘读取指定磁道、磁头上的1~256个扇区到主机。送到主机的
		数据可以添加4个字节的ECC校验码，读的起始扇区号和扇区个数在命令块指定。这条命令也隐藏
		着寻找指定的磁道号，所以不需要另外的寻道命令。
	30H 写扇区命令(带重试)：本命令是将主机内的数据写入硬盘，可以写指定磁道、磁头上的
		1~256个扇区，与读扇区命令相似，这条命令也隐藏着寻找指定的磁道号，写的起始扇区号和扇区
		个数由命令块指定。
	90H 硬盘诊断命令：以判断硬盘是否已经连接到主机上，可以读取错误寄存器以检查需要的
		结果，如果是01H或81H表示设备是好的，否则表示设备没有连接或设备是坏的。
		设备控制寄存器：将该寄存器的SRST位设置为1，可以使硬盘驱动器处于复位状态。 IEN表示
		是否允许中断，其中0为允许。由此可见，对该寄存器发送0X0CH命令即令硬盘复位，其格式如表7
		所示。
	*/
    outb(0x1F7, 0x20);                      // cmd 0x20 - read sectors

    // wait for disk to be ready
    waitdisk();

    // read a sector
	// 使用0x1F0数据寄存器进行数据传输的方式叫PIO（Programing Input Output）
	// 数据交换的另一种方式是使用DMA。这种方式不使用数据寄存器进行数据交换。
    insl(0x1F0, dst, SECTSIZE / 4);
}

/* *
 * readseg - read @count bytes at @offset from kernel into virtual address @va,
 * might copy more than asked.
 * */
static void
readseg(uintptr_t va, uint32_t count, uint32_t offset) {
    uintptr_t end_va = va + count;

    // round down to sector boundary
	// va后移保证扇区开始端对齐0x10000这个位置
    va -= offset % SECTSIZE;

    // translate from bytes to sectors; kernel starts at sector 1
	// secno表示扇区号
    uint32_t secno = (offset / SECTSIZE) + 1;

    // If this is too slow, we could read lots of sectors at a time.
    // We'd write more to memory than asked, but it doesn't matter --
    // we load in increasing order.
    for (; va < end_va; va += SECTSIZE, secno ++) {
        readsect((void *)va, secno);
    }
}


/* bootmain - the entry of bootloader */
void
bootmain(void) {
    // read the 1st page off disk
	// 读取磁盘上的8个扇区到ELFHDR指向的内存单元处
    readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0);

    // is this a valid ELF?
    if (ELFHDR->e_magic != ELF_MAGIC) {
        goto bad;
    }

    struct proghdr *ph, *eph;

    // load each program segment (ignores ph flags)
	// 根据程序中的‘程序头表’直接将可执行代码加载至指定位置。
    ph = (struct proghdr *)((uintptr_t)ELFHDR + ELFHDR->e_phoff);
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph ++) {
        readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
    }

	
    // call the entry point from the ELF header
    // note: does not return
	// 在第一个扇区的kernel代码中，程序入口点已经设置为init.c中的kern_init函数
	// 具体见tools/kernel.ld文件。所以这里其实是调用kern_init()这个函数。
    ((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();

	
	
	/*
	bootmain 的工作就是加载并运行内核。只有在出错时该函数才会返回，
	这时它会向端口 0x8a00（8470-8476）输出几个字。在真实硬件中，
	并没有设备连接到该端口，所以这段代码相当于什么也没有做。
	如果引导加载器是在 PC 模拟器上运行，那么端口 0x8a00 则会连接到模拟器并把控制权交还给模拟器本身。
	无论是否使用模拟器，这段代码接下来都会执行一个死循环（8477-8478）。
	而一个真正的引导加载器则应该会尝试输出一些调试信息。
	*/
bad:
    outw(0x8A00, 0x8A00);
    outw(0x8A00, 0x8E00);

    /* do nothing */
    while (1);
}

