# OS 简介与相关接口

## OS的基本功能

OS所处的位置在指令集之上，在应用层之下，基于指令集，保证应用程可以正常运行，并为应用提供更加高效有用的接口，这就是OS的核心作用。

> 指令集(ISA)：区别于最基础的位运算和寄存器操作，现代处理器为了方便底层开发，会将大量的常用指令集和整合为统一的指令，称之为指令集，是汇编的系统集合与构成。

根据面向对象的设计思想，在不同层级之间交互的时候，使用的应该是接口，而接口自然而然的具备抽象的特性，对于**应用层**而言，OS对*硬件层*进行了抽象，举个例子：一个现代操作系统应当具备多进程支持，故需要对CPU的时间片进行合理分配，使得不同的进程都可以得到时间运行，进而达到多进程支持，但是CPU多种多样，多核CPU，单核CPU，大小核CPU的核心调度策略理应不同，而在应用层的应用并不需要了解这些，OS将对上述一切进行抽象，应用只需要知道它一定可以被执行到就可以了。

## 常见的System Call提供方式

* 微内核：部分的System Call运行在User Space，部分运行在Kernel Space，这样做的优点在于减少了Kernel的冗杂度，降低了Kernel Space的负担，理论上可以减少内核报错的概率。
* 常规内核：所有System Call均运行在Kernel Space，设计简单，process隔离简单，目前Linux就处于这种模式。

## fork()详细分析

fork的作用是将进程复制，复制的内容几乎进程的全部（pid除外），但是值得注意的是，子进程复制过后的内存虽然和父进程内容上相似（contains the same），但是实际上存储的内存位置和进程调用的寄存器位置是完全不同的，也就是内容上的复制，但是实际指针（内存中的存储位置）是两个。

> changing a variable in one does not affect the other

### fork的双返回值

fork的返回值：如果是父进程，则返回值为子进程id，如果是子进程，则返回值为0。需要注意的是，如果进行一次fork，将会出现两次返回，这是因为在fork的过程中，进程的程序计数器同样会被复制，当子进程被复制之后，和父进程一样，程序计数器都处在fork之中，故从宏观上讲，在整个OS中将会有两个程序计数器指向fork函数之中，就会有两次fork的返回值。

## exec()详细分析

exec作用是将特定的程序内存image替换掉当前的process内存。其中的内存image是从file中读取的，这就要求file需要具备一定的格式，类似于编程语言中要求要有一定的文法规则一样，在xv6中采用的是ELF（可执行与可链接）文件格式

> ELF与c的编译过程（就不详细的学习编译原理课程了
>
> C语言的执行流程为：
>
> * 预处理阶段（CPP）：主要进行文本替换、宏展开和注释删除。产生.i文件（文本文件）
> * 编译阶段：将文本文件编译产生汇编程序（.s）
> * 汇编阶段：将.s文件的汇编程序翻译为机器码，生成一种**可重定位目标程序**，文件格式为.o
> * 链接阶段：编译过后的.o文件绝不是独立的，彼此之间往往存在调用关系，故通常需要将不同的程序链接在一起，使得主调函数可以调用到被调函数。分为静态链接和动态链接两种，也就是动态库和静态库，静态库的后缀一般为.a，动态库的后缀一般为.so（貌似可以鲁莽的认为java属于动态链接）
>
> 常见的目标文件形式
>
> * 可重定位目标文件：包含二进制代码与数据，由汇编器产生，多个可重定位目标文件经过链接器之后形成可以被运行的可执行目标文件
> * 可执行目标文件：包含二进制代码和数据，可以被加载器直接执行。
>
> **To be continued =>**

exec在执行完成进程之后并不会返回到调用exec的程序，貌似是会在ELF文件头指明的位置执行。（xv6书这里写的比较费解，就姑且理解成，exec会用ELF文件完全替换掉当前process的内容（当然在后面的章节中可以了解到，其实其中的**文件标识符**是没有被替换掉的）就将导致其返回值位置（理应写在stack中）也会被修改，最终导致process无法返回到主调位置）

`This fragment replaces the calling program with an instance of the program /bin/echo running with the argument list echo hello. `，书中的这部分内容感觉就是在暗示这种“洗内存”的感觉

exec接收两个参数，分别是函数文件名和string参数数组。一般而言，string数组的第一个参数表示被调函数的名称，所以一般这个不发挥作用。

### Shell的执行（含源码分析）

Shell不断的接收参数并执行参数，其具体的执行内容如下

sh.c的main函数

```c
int
main(void) {
    static char buf[100];
    int fd;

    // Ensure that three file descriptors are open.
    // 保证三个最基本的文件标识符都是打开的
    while ((fd = open("console", O_RDWR)) >= 0) {
        if (fd >= 3) {
            close(fd);
            break;
        }
    }

    // Read and run input commands.
    // 读取并且执行输入的指令
    while (getcmd(buf, sizeof(buf)) >= 0) {
        
        //貌似是在单独处理cd 指令
        if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
            // Chdir must be called by the parent, not the child.
            buf[strlen(buf) - 1] = 0;  // chop \n
            if (chdir(buf + 3) < 0)
                fprintf(2, "cannot cd %s\n", buf + 3);
            continue;
        }
        
        //这里的fork1好像是保证可以正常fork的fork
        //就是将异常的fork（pid == -1）直接panic掉
        //保证这里拿到的是正确的子进程
        if (fork1() == 0)
            runcmd(parsecmd(buf));
        wait(0);
    }
    exit(0);
}
```

可以看到调用栈指向了runcmd函数，这个函数接收一个cmd结构体，看起来是需要将入参通过parsecmd函数打包为cmd结构体，简单说明一下cmd结构体：cmd结构体貌似只是对指令进行了简单的分类，将不同的指令写作了不同的type，用于在exec的时候执行

重点还是应当着眼于runcmd函数，下为源码及其分析

值得注意的是，因为在上文中已经调用了fork函数，这里的执行已经在子线程中了，已经与维持shell的原始进程不再是同一个进程

```c
// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd) {
    int p[2];
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if (cmd == 0)
        exit(1);

    switch (cmd->type) {
        default:
            panic("runcmd");

        case EXEC:
            ecmd = (struct execcmd *) cmd;
            //注意，下面并不是意味着exec没有输入参数就将不执行
            //就算函数没有入参，比如在控制条输入cat之后回车
            //其string args中仍然存在一个与函数同名的参数
            //故这里的argv[0] == 0表示最基础的第一个参数错误，故exit
            if (ecmd->argv[0] == 0)
                exit(1);
            exec(ecmd->argv[0], ecmd->argv);
            //这里很细节，在正常情况下exec指令的调用结果并不会返回到主调函数中
            //如果下面的函数执行了，就必然表示exec执行错误
            //xv6这里的处理就是通过标准输出流标识执行失败
            fprintf(2, "exec %s failed\n", ecmd->argv[0]);
            break;

        case REDIR:
            rcmd = (struct redircmd *) cmd;
            close(rcmd->fd);
            if (open(rcmd->file, rcmd->mode) < 0) {
                fprintf(2, "open %s failed\n", rcmd->file);
                exit(1);
            }
            runcmd(rcmd->cmd);
            break;
            
            ....省略部分代码....
    }
    exit(0);
}
```

需要后面留意的是，在exec之前，子进程可以修改自己的文件标识符，这样的结果是可以减少被执行的file本身的压力，它不需要去单独处理多种文件标识符的情况，对于它来说，文件标识符就是抽象的标准输入和标准输出，算是一定程度上的抽象。这同时也是为什么fork和exec不能同时执行的重要原因

接下来将要介绍文件标识符和IO，这里盲猜shell的回显是通过IO或者pipe实现的

## 文件标识符与I/O

文件标识符：File Descriptors：

### 万物皆文件

首先需要了解一个概念，在Linux或者Unix Like的系统中，万物皆文件，也就是说这里的文件标识符的作用远比它看起来要大，实际上也是如此。

一般而言，文件标识符0表示标准输入流，文件标识符1表示标准输出流，文件标识符2表示标准错误输出流。用于和file descriptors交互的函数往往是read or write，open or close，前一对的作用为：从指定的输入流读取一定量的数据，从指定的输出流输出一定的数据。后者则是打开or关闭一个fd（具体的规则后续介绍）

总结的说，fd联系着一个文件，并且内部存在一个offset，这个offset，fd将会依据offset在文件的指定位置读取和写入。



`The important thing to note in the code fragment is that cat doesn’t know whether it is reading from a file, console, or a pipe. Similarly cat doesn’t know whether it is printing to a console, a file, or whatever. The use of file descriptors and the convention that file descriptor 0 is input and file descriptor 1 is output allows a simple implementation of cat.`



这段原文突出了fd的抽象性，即fd并不一定指向的是一段txt like的文本，它更有可能是pipe，console等等其他的东西，一方面，这体现了万物皆文件的抽象，一方面，这方便了cat的程序设计，也就是说程序的设计者并不用了解如果fd来自pipe要怎么做，来自console要怎么做，它只需要关注输入和输出就可以了，其他的均与他无关。

### open与close的规则

使用close函数可以放飞一个fd，标识它为可用，之后当使用open函数打开file时，系统会自动分配最小的fd用于指向对应的文件。

`The close system call releases a file descriptor, making it free for reuse by a future open, pipe, or dup system call (see below). A newly allocated file descriptor is always the lowest- numbered unused descriptor of the current process.`

### 使用fd进行I/O重定向

根据已知信息，exec会洗掉内存，但是并不会影响程序的文件标识符（`The system call exec replaces the calling process’s memory but
preserves its file table.`）这样的话，在fork之后，exec之前，就可以根据父进程的fd修改子进程的fd，之后执行exec，实现I/O的重定向。（这也是为什么fork和exec不应该同时执行的原因）。下为例子

```c
char *argv[2];
argv[0] = "cat";
argv[1] = 0;
if(fork() == 0) {
	close(0);//在子进程中关闭标准输入流
   //根据close和open的规则，这里是将标准输入流重定向到input.txt
	open("input.txt", O_RDONLY);
   //重定向之后再执行exec，并不会洗掉fd，故可以实现重定向
	exec("cat", argv);
}
```

I/O重定向的好处是什么？

`the shell has a chance to redirect the child’s I/O without disturbing the I/O setup of the main shell. `即可以在不干扰主程中原本的fd指向的情况下修正逻辑，可以满足一定的抽象原则。

### dup指令

dup会生成一个和输入fd指向同一个file的文件，并且二者共享offset

```c
fd = dup(1);
write(1, "hello ", 6);
write(fd, "world\n", 6);
```

最终的结果将是hello world\n，值得注意的是，用任意其他方法打开的fd均不可能共享offset，就算是对同一个file open两次，最终得到的两个fd也不会共享offset，如果是其他的情况，最终的结果只会是world\n

这样的作用是什么呢？



`Dup allows shells to implement commands like this: ls existing-file non-existing-file > tmp1 2>&1. The 2>&1 tells the shell to give the command a file descriptor 2 that is a duplicate of descriptor 1. Both the name of the existing file and the error message for the non-existing file will show up in the file tmp1.`



上文的大体含义是：这样的话支持同时将多个fd的内容灌入同一个输出之中，例如将报错信息和正确信息同时灌入一个log文件之中，并且保证二者的offset相同，不会互相覆盖



## pipe 管道

管道是一组文件标识符，其中一个的输入对应到另一个的输出，构成双工通信的两条链路，通过此可以实现不同process之间的通信。具体的使用方法如下

```c
int p[2];//用于承载pipe两个文件标识符的数组
char *argv[2];
argv[0] = "wc";
argv[1] = 0;
pipe(p);//基于p建立一个pipe
if(fork() == 0) {
	close(0);//将标准输入流close
	dup(p[0]);//复刻一个p[0]，因为这个时候空闲的是0，所以是将0指向p[0]
	close(p[0]);//关闭两个pipe
	close(p[1]);
    //目前0指向p[0]的dup，也就是pipe的输出端（read side）
    //程序的标准输入流将指向pipe的输出端（read side）
    //也就是将管道的输出灌入子进程的标准输入
	exec("/bin/wc", argv);
} else {
    //父进程中有和子进程完全相同的文件标识符
    //也就是说这里的p和子进程完全相同
	close(p[0]);//关闭父进程中管道的read端
	write(p[1], "hello world\n", 12);//向管道的write端灌入字符串
	close(p[1]);//关闭管道
}
```

这里比较值得留意的是管道什么时候可以read出东西，原书中是这样给出的



`If no data is available, a read on a pipe waits for either data to be written or for all file descriptors referring to the write end to be closed`



也就是写入了数据或者等到整个write side被关闭了，都将导致read被唤醒，这也是上面的那套代码中最终关闭pipe[1]的原因之一。

### 管道符的实现

当然，在实际的shell使用中，并不是靠pipe指令建立pipe的，现代的shell均可以通过管道符“|”去建立pipe，其具体的逻辑代码实现如下

```c
case PIPE:
//检测到是PIPE类型的语句
pcmd = (struct pipecmd *) cmd;
if (pipe(p) < 0)//尝试建立管道，如果建立失败则panic
    panic("pipe");
if (fork1() == 0) {
    //开辟子进程1
    close(1);//关闭标准输出流
    dup(p[1]);//使得1指向管道的write side，也就是将标准输出流灌入管道的输入端
    //关闭管道
    close(p[0]);
    close(p[1]);
    //在子进程1中运行左侧指令
    //指令的输出->标准输出->管道write side
    runcmd(pcmd->left);
}
if (fork1() == 0) {
    //开辟子进程2
    close(0);//关闭标准输入流
    dup(p[0]);//使得0指向管道的read side，也就是将管道的输出灌入标准输入流
    //关闭管道
    close(p[0]);
    close(p[1]);
    //执行右侧指令
    //管道输出->标准输入->指令的输入
    runcmd(pcmd->right);
}
close(p[0]);
close(p[1]);
wait(0);
wait(0);
break;
```

我不太确定上述代码是如何保证代码执行的顺序性的，不过猜测应该是右侧代码在没有输入时不会执行

当然，在右侧runcmd的时候，这里的cmd依旧可以是一个pcmd（含管道符的cmd）
