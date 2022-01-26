# GCC编译过程、伪操作、ELF
为了契合OS的学习，简单补充一点GCC编译流程、汇编和ELF的知识

> 参考文章：[ELF文件解析（一）：Segment和Section](https://segmentfault.com/a/1190000016664025)	[ELF_Format.pdf (skyfree.org)](http://www.skyfree.org/linux/references/ELF_Format.pdf)
>
> [汇编程序伪操作指令_Eddy的博客-CSDN博客_汇编伪操作](https://blog.csdn.net/qq_41596356/article/details/121539963)	[ELF文件详解—初步认识](https://blog.csdn.net/daide2012/article/details/73065204)
>
> [07 链接 | Sun (sunyi720.github.io)](https://sunyi720.github.io/2019/07/24/系统原理/07 链接/)	[ELF文件装载链接过程及hook原理](https://felixzhang00.github.io/2016/12/24/2016-12-24-ELF文件装载链接过程及hook原理/)

## 伪操作

汇编程序中以`.`开头的名称并不是指令的助记符，不会被翻译成机器指令，而是给汇编器一些特殊指示，称为汇编指示（Assembler Directive）或伪操作（Pseudo-operation），常见的伪操作如下

* `.section`: 此指令将汇编划分为多个段，在程序执行的时候，不同的段将加载到不同的地址，进行不同的读写操作和执行权限，一般需要结合其他伪操作使用

  > section出现在代码中，每一个section都会对应一个section header table，用于链接器进行重定位

* `.text`（`.section .text`）: 后面的段用于保存代码

* `.data`（`.section .data`）: 保存已经初始化的全局静态变量和局部静态变量，值得注意的是，c中的局部变量既不在.data中，也不再后文的.bss中，其存在于进程执行产生的堆栈中。此外，如果变量被初始化值为0，也可能会放到bss段

* `.rodata`（`.section .rodata`）: 存放的是只读数据，一般是程序的只读变量和字符串常量，如C语言中的`const`修饰的变量，存放于此段中不仅可以契合c的语法特性，增加只读段也可以提高OS的安全性

* `.bss`（`.section .bss`）：`bss`段保存未初始化的全局变量和局部静态变量。如果静态变量有被初始化，或者初始化之后的结果为0，皆适合存放于bss段中

* `.symtab`（`.section .symtab`）: `symtab`段保存符号表，也就是整个汇编代码中应用到的符号在内存中的具体位置。

* `.init` & `.finit`: 保存初始化与终结代码段，前者类似于java的静态代码块，后者类似于死亡回调

* `.global`: 后面跟一个符号，比如一个函数名，`.global`会告诉汇编器，后面这个符号将在链接器中使用，比如下面这个例子：

  ```assembly
  # _entry为预想中的入口函数
  .global _entry
  ```

  用`.global`修饰了_entry，后者就可以在链接脚本中如此写来明确入口函数位置

  ```
  ENTRY(_entry)
  ```

## ELF

ELF是`Executable and Linking Format`的缩写，即可执行和可链接的格式，是Unix/Linux系统ABI (Application Binary Interface)规范的一部分。从名字中就可以看出来，ELF是需要面向两个不同的方向的，Executable表明其需要面向OS的应用执行，Linking则代表它可以面向链接器。

> 提前切入一些链接的相关知识：根据上文中对汇编的描述，可以大概认为在机器中，没有变量和函数的区别，二者统称为符号，每一个section中都含有自己的符号，并缓存在自己的符号表中。如果引用了外部符号，在使用的时候就需要解析符号，找到符号定义的位置，并关联起来，这个过程就是符号解析。
>
> 实现符号解析的其中一种策略就是重定位：未合并的代码的位置和内存往往是不确定的，重定位的目的就是合并代码，并确定内存地址。整个链接过程分两步：
>
> - 第一步 空间与地址分配 扫描所有的输入目标文件，并且获得它们的各个段的长度、属性和位置，并且将输入目标文件中的符号表中所有的符号定义和符号引用收集起来，统一放到一个全局符号表中。
> - 第二步 符号解析与重定位 使用第一步中收集到的信息，读取输入文件中段的数据、重定位信息，并且进行符号解析与重定位、调整代码中的地址等

下面是在两个不同视角下（链接与执行），ELF文件的格式

<img src="https://img-blog.csdn.net/20170611205621669?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvZGFpZGUyMDEy/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/Center" style="zoom:70%;" /> 

链接注重的是section的不同条目，重点在于将不同的条目融合在一起，看到的是一个一个的section，exec注重的是如何执行，重点在于：代码、数据、符号，看到的是一个一个的segment（虽然理论上依旧是一个segment对应一个section）

![](https://img2018.cnblogs.com/blog/417313/201810/417313-20181012154909093-954664315.png) 

## GCC编译过程

大体可以分为四个过程：预处理、编译、汇编、链接
### 预处理
这部分并不是真正的编译过程，输出的内容依旧是代码。主要进行以下工作：
* 代码的宏展开：将定义的宏替换为重复的代码
* 文件包含：比如将`_FILE_`替换为当前文件
* 代码删除：删除对机器而言无用的代码，比如注释和编译优化
gcc只进行预处理的指令如下
```shell
gcc -E xxx.c -o xxx.i
```
下面这串代码是main.c源代码（为了防止因stdio引起的预处理后代码过长，这里就不引入任何头文件了）
```c
int sum(int *a, int n);
int main(){
	int array[2] = {1, 2};
	int val = sum(array, 2);
	return val;//这是一行愚蠢的注释
}
```
经过预处理之后其输出为
```c
# 1 "main.c"
# 1 "<built-in>"
# 1 "<command-line>"
# 31 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 32 "<command-line>" 2
# 1 "main.c"
int sum(int *a, int n);
int main(){
 int array[2] = {1, 2};
 int val = sum(array, 2);
 return val;
}
```
明显可见注释被删除了，以上过程称为预处理
### 编译
将预处理之后的代码转换为汇编的过程，值得注意的是，这里输出的是汇编，而非二进制，指令如下
```shell
gcc -S xxx.i -o xxx.s
```
比如在上面的例子中，输出结果如下
```assembly
.file	"main.c"
	.text
	.globl	main
	.type	main, @function
main:
.LFB0:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$32, %rsp
	movq	%fs:40, %rax
	movq	%rax, -8(%rbp)
	xorl	%eax, %eax
	movl	$1, -16(%rbp)
	movl	$2, -12(%rbp)
	leaq	-16(%rbp), %rax
	movl	$2, %esi
	movq	%rax, %rdi
	call	@PLT
	movl	%eax, -20(%rbp)
	movl	-20(%rbp), %eax
	movq	-8(%rbp), %rdx
	xorq	%fs:40, %rdx
	je	.L3
	call	__stack_chk_fail@PLT
	
...省略部分代码...
```
简单汇编相关内容会后续介绍
### 汇编
此处的目的为将汇编代码替换为二进制机器码，指令如下
```c
gcc -c xxx.s -o xxx.o
```
在上面的例子中，这样之后的输出结果为
<img src="https://github.com/RayleighZ/ImageBed/blob/master/assamble.png?raw=true" style="zoom:50%;" /> 
值得注意的是，最前面的0x457f464c就是ELF magic，（这是一个ELF文件）

### 链接

经过上述步骤之后的每一个.c文件都将会变成一个一个的机器码，然而很多情况下这些机器码之间是有相互交互的，比如共用一些全局变量，比如相互之间存在函数调用，这里的例子中，main.c就调用了一个名为sum的函数，如果sum函数在它处实现，则如何告诉系统其位于何处？这时就需要进行链接

常规意义上，链接执行的功能为：将多个输出文件合并为一个输出文件，这其中就要涉及代码的合并、变量表的合并等等，可以自写ld脚本，也可以使用默认。链接的指令如下

```shell
ld -r a.o b.o -o final.o
```

比如在这里的例子中，需要将`main.o`与`sum.o`链接起来，需要执行的代码如下

```shell
ld -r main.o sum.o -o final.o
```

最终就会输出`final.o`。让我们分析比较一下以上三个ELF文件，以下分别是`main.o sum.o final.0`的`readelf`输出中，符号表的部分：

```shell
Symbol table '.symtab' contains 13 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0000000000000000     0 FILE    LOCAL  DEFAULT  ABS main.c
     2: 0000000000000000     0 SECTION LOCAL  DEFAULT    1 
     3: 0000000000000000     0 SECTION LOCAL  DEFAULT    3 
     4: 0000000000000000     0 SECTION LOCAL  DEFAULT    4 
     5: 0000000000000000     0 SECTION LOCAL  DEFAULT    6 
     6: 0000000000000000     0 SECTION LOCAL  DEFAULT    7 
     7: 0000000000000000     0 SECTION LOCAL  DEFAULT    8 
     8: 0000000000000000     0 SECTION LOCAL  DEFAULT    5 
     9: 0000000000000000    86 FUNC    GLOBAL DEFAULT    1 main
    10: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT  UND _GLOBAL_OFFSET_TABLE_
    11: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT  UND sum
    12: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT  UND __stack_chk_fail
```

```shell
Symbol table '.symtab' contains 10 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0000000000000000     0 FILE    LOCAL  DEFAULT  ABS sum.c
     2: 0000000000000000     0 SECTION LOCAL  DEFAULT    1 
     3: 0000000000000000     0 SECTION LOCAL  DEFAULT    2 
     4: 0000000000000000     0 SECTION LOCAL  DEFAULT    3 
     5: 0000000000000000     0 SECTION LOCAL  DEFAULT    5 
     6: 0000000000000000     0 SECTION LOCAL  DEFAULT    6 
     7: 0000000000000000     0 SECTION LOCAL  DEFAULT    7 
     8: 0000000000000000     0 SECTION LOCAL  DEFAULT    4 
     9: 0000000000000000    73 FUNC    GLOBAL DEFAULT    1 sum
```

```shell
Symbol table '.symtab' contains 14 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0000000000000000     0 SECTION LOCAL  DEFAULT    1 
     2: 0000000000000000     0 SECTION LOCAL  DEFAULT    2 
     3: 0000000000000000     0 SECTION LOCAL  DEFAULT    4 
     4: 0000000000000000     0 SECTION LOCAL  DEFAULT    6 
     5: 0000000000000000     0 SECTION LOCAL  DEFAULT    7 
     6: 0000000000000000     0 SECTION LOCAL  DEFAULT    8 
     7: 0000000000000000     0 SECTION LOCAL  DEFAULT    9 
     8: 0000000000000000     0 FILE    LOCAL  DEFAULT  ABS main.c
     9: 0000000000000000     0 FILE    LOCAL  DEFAULT  ABS sum.c
    10: 0000000000000056    73 FUNC    GLOBAL DEFAULT    2 sum
    11: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT  UND __stack_chk_fail
    12: 0000000000000000    86 FUNC    GLOBAL DEFAULT    2 main
    13: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT  UND _GLOBAL_OFFSET_TABLE_
```

就可以很开心的发现，前两者的符号表被成功的链接到了最后的final中，这时候只需要再将`final.o`转换为可执行文件，就能直接调用

```shell
gcc final-main.o -o final-main
./final-main
echo $?
输出为：
3
```

