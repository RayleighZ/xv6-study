# OS大致规划

## OS的基本需求概述

三大基本要求：并发、内存隔离、可交互性

> 虽然说是基本需求，但也并不是所有的OS都严格满足上述需求，早期的MacOS内核并不具备内存隔离，用户进程可以修改kernel的内存，其因用户态程序错误导致crash的可能性是蛮高的（应当很敬佩当时的开发者，在一不小心就有可能引起系统crash的情况下，写出了如此多复杂的程序）

## 物理资源虚拟化

物理资源虚拟化的主要目的是使得用户态的程序可以不直接接触到硬件，这样做的好处我理解有以下几点

1、减少程序负担：就算指令集相同，不同硬件资源的处理器核心数，内存大小等都是不确定的，比如一个涉及到并行需求的程序，可能就需要对单核CPU与多核CPU做单独处理。

> 当然，上述业务需求中的抽象也可以在应用内进行，比如应用内部将CPU的核心抽象成“任务处理能力”，类似在应用层实现类似Java中Tread一般的系统库，这并未跳脱物理资源虚拟化的范围，但很显然，这个工作应该被放在OS中进行，这样可以帮助大量的程序解耦，减轻程序负担

2、方便OS集中处理：三大基本需求中的并发需求，需要对CPU时隙进行流转，这就需要对CPU进行抽象，并交由OS进行轮转处理。三大基本需求中的内存隔离需求，如果不让用户态程序可以直接交互到物理内存，而是交由用户进程一个基于物理内存的虚拟内存（后文的会详细介绍），则可以更加方便的实现内存隔离。

其中个人认为，方便OS集中处理是最核心的需求，毕竟是实现核心需求的基本框架。

`The downside of this library approach is that, if there is more than one application running, the applications must be well-behaved. For example, each application must periodically give up the CPU so that other applications can run. Such a cooperative time-sharing scheme may be OK if all applications trust each other and have no bugs. It’s more typical for applications to not trust each other, and to have bugs, so one often wants stronger isolation than a cooperative scheme provides.`

上面是the book中对用户态进程直接交互硬件资源的劣势的说明，核心点就在并发的处理上，让用户进程占据硬件资源，时间片的轮转将会相当困难。

一言以蔽之，物理资源虚拟化给了OS操作空间

## Machine Mode, User Mode, Supervisor Mode

为了实现Isolation，权限控制必不可少，RISC-V提供了三种不同的CPU工作模式，MM，UM，SM。其中MM有完备的权力，CPU自MM启动，一般OS在MM下进行一些电脑的基础配置，xv6中，OS只在MM下工作几行，就会切换到SM。OS在执行System Call等需要一定权限的操作时，并不会直接运行指令，而是会切换到SM之后再运行，反映在OS中，就是OS必须要从用户层切换到Kernel中去执行系统调用。

`CPUs provide a special instruction that switches the CPU from user mode to supervisor mode and enters the kernel at an entry point specified by the kerne`

下面是一句废话（转移到SM的入口需要归Kernel所有）

` It is important that the kernel control the entry point for transitions to supervisor mode`

根据OS是否完全在内核中，可以将OS分为整体式内核和微内核。

这里值得一提Process在XV6中的结构体

```c
// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state 进程状态
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID 进程ID

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process 父进程

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack Kernel Stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table 虚拟内存页表
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};

```

每一个进程都有两个stack，分别是user stack和kernel stack，当进程运行普通的用户态指令时，将在user stack执行，当进程运行需要kernel权限的指令时，将会切换到内核，之后内核的代码将在kernel stack运行（并不是进程通过kernel stack运行kernel级别指令，而是进程委托kernel，让kernel在自己的kernel stack上执行kernel指令），很明显，kernel stack中运行的数据也属于kernel的一部分，所以kernel stack是和用户态process隔离的，并受到保护，防止被用户代码侵犯

## xv6启动源码

我电脑上的xv6是运行在qemu里面的，所以从make qemu指令切入xv6的编译与启动过程是个不错的选择，make指令来自make file，故入手点就应当是xv6项目中的MAKEFILE。

> 稍微加一点Make file的相关知识（大学C语言课程基本上不讲这个，也就算是告别工程化项目实践了，我也是在学xv6的时候才第一次接触到Make file）
>
> 在前文介绍ELF的时候，有提到C文件形成可执行文件的过程，其中编译和链接是必不可少的内容，如果手头上有一大堆.c文件，编译器其实是不知道要编译和链接那些内容的，这时候就需要有什么东西来指挥编译和链接的过程，如果使用的是Clion新建新项目，发挥这个功能的一般是CMake，而在更广泛的使用场景中，担任编译和链接指路人工作的是MakeFile，xv6中担任编译和链接引路作用的就是MakeFile。MakeFile定义了那些文件需要被编译，那些文件需要被链接，将零散的.c文件汇编结果构成最终的kernel

下面是xv6的MakeFile，重要的部分通过注释说明下。

```makefile
# 宏定义
K=kernel
U=user
# 宏定义后面链接需要的汇编文件
# 如果以下文件中某一个不存在，则会先编译产生文件
OBJS = \
  $K/entry.o \
  $K/start.o \
  $K/console.o \
  $K/printf.o \
......
  
LDFLAGS = -z max-page-size=4096

# 从make qemu跳转到这里
# 先编译OBJS, 通过kernel.ld链接, 并且编译initcode
# 这里主要关注kernel.ld
$K/kernel: $(OBJS) $K/kernel.ld $U/initcode
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS) 
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

$U/initcode: $U/initcode.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/initcode.S -o $U/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/initcode.out $U/initcode.o
	$(OBJCOPY) -S -O binary $U/initcode.out $U/initcode
	$(OBJDUMP) -S $U/initcode.o > $U/initcode.asm

......

# 这里是在shell键入make qemu之后会执行到的地方
# 如果是第一次启动，很显然是没有kernel和fs.img的
# 故需要形成K/Kernel
qemu: $K/kernel fs.img
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $K/kernel .gdbinit fs.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)
```

执行流程为：`make qemu`->编译kernel->编译OBJS->执行`kernel.ld`与`initcode.S`

首先看一下`kernel.ld`，代码如下

```s
OUTPUT_ARCH( "riscv" )
/**
 * 设置了整个System的入口为_entry
 */
ENTRY( _entry )

SECTIONS
{
  /*
   * ensure that entry.S / _entry is at 0x80000000,
   * where qemu's -kernel jumps.
   * 这里是qemu认可OS可调用的第一个地址，如果看了后文的话，应该晓得低地址是留给硬件的
   */
  . = 0x80000000;

  ......

  PROVIDE(end = .);
}
```

这里可以看到入口函数为_entry，这个函数位于`entry.S`中，代码如下

```assembly
	# qemu -kernel loads the kernel at 0x80000000
        # and causes each CPU to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.global _entry
_entry:
	# set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)
        la sp, stack0
        li a0, 1024*4
	csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
	# jump to start() in start.c
        call start
spin:
        j spin

```

_entry函数的具体内容与解释如下

```assembly
# stack0的定义在start.c中，代表栈，本质上是一个数组
# 其具体大小是最大cpu数*4096
# sp寄存器是堆栈寄存器，将栈的头指针地址塞入其中，方便后续寻址
# sp = stack0 + (hartid * 4096)
la sp, stack0
li a0, 1024*4
# 在a1处存储当前执行的线程的id
csrr a1, mhartid
addi a1, a1, 1
mul a0, a0, a1
# 根据sp = stack0 + (hartid * 4096)的规则
# 计算目前的sp应该指向的位置
add sp, sp, a0
# 在栈准备好之后，开始调用start.c中的start()函数
call start
```

接下来就是start函数，值得一提的是，直到目前，系统都处于Machine Mode，在start函数中，将完成MM到SM的切换，以及SM到UM的切换。剩余内容在trap章节会详细介绍，这里就看一眼

```c
void
start()
{
  // set M Previous Privilege mode to Supervisor, for mret.
  // 将MM设置为SM
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  w_mepc((uint64)main);

  // disable paging for now.
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id);

  // switch to supervisor mode and jump to main().
  asm volatile("mret");
}
```