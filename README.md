# 一个能够自举的C编译器

如题实现一个能够自举的玩具C编译器(缩水版的C，只支持其中一部分特性)。

参考资料：
- 直接学习：[Bilibili-700行手写编译器](https://www.bilibili.com/video/BV1Kf4y1V783)
- 间接参考：[rswier/C4](https://github.com/rswier/c4)，一个500行的能够自举的简易C语言编译器。
- 更多关于C4以及实现一个C编译器该有的细节：[知乎-RednaxelaFX关于C4的文章](https://www.zhihu.com/question/28249756/answer/84307453)

## 特性


## 编译器

先看一下Wiki：
>将便于人编写、阅读、维护的高级计算机语言所写作的源代码程序，翻译为计算机能解读、运行的低阶机器语言的程序，也就是可执行文件。编译器将原始程序（source program）作为输入，翻译产生使用目标语言（target language）的等价程序。源代码一般为高级语言（High-level language），如Pascal、C、C++、C# 、Java等，而目标语言则是汇编语言或目标机器的目标代码（Object code），有时也称作机器代码（Machine code）。
一个现代编译器的工作流程：源代码（source code）→ 预处理器（preprocessor）→ 编译器（compiler）→ 汇编程序（assembler）→ 目标代码（object code）→ 链接器（linker）→ 可执行文件（executables）。

一个工业级别的编译器的编译过程：

前端部分：源代码 到 中间代码，中间代码通常来说是一个抽象语法树AST，也可以是LLVM(IR)
- 词法分析（tokenize）：将所有标识符、运算符、关键字等解析为具有含义的记号(token)。比如int解析为一个表示类型的记号。
- 语法分析（parsing）：将token组织成完成的语句（statement），用AST来表示。比如将一个1*2+3这样的表达式就会按照运算优先级组织成一个类似如下的表达式树。
    ```
        +
       / \
      *   3
     / \
    1   2
    ```

后端部分：中间代码 到 目标代码
- 一个可选的优化器（optimizer）：中间代码到中间代码，完成一系列的优化过程。比如常量替换、函数内联、死代码剔除（DCE, Dead code elimination）等。工业编译器最核心也是最复杂的部分。
- 代码生成（CodeGen）：中间代码到目标代码，目标代码就是目标平台的汇编比如x86/ARM/MIPS汇编，或者某一个虚拟机的指令如Java字节码。
- 著名开源项目[LLVM](https://llvm.org/)提供了一套适合编译器系统的中间语言（Intermediate Representation，IR），有大量变换和优化都围绕其实现。经过变换和优化后的中间语言，可以转换为目标平台相关的汇编语言代码。而后端又是非常繁琐的事情，所以实现编译器时可以考虑实现前端，生成LLVM IR，将后端交给LLVM去做。


## VM设计

编译器设计思路：
- 前后端合一，无中间优化。
- 目标代码基于自定义的虚拟机（VM），非常简单但也足够完整。
- One-Pass 解析过程：源代码经过Parse & CodeGen过程直接生成虚拟机指令，然后由虚拟机来执行。
- 由此某些特性无法实现：比如说在必须将局部变量声明在函数一开始的位置。

### 总览

极简设计的虚拟机；
- 基于寄存器和栈。只有一个通用寄存器ax，一元运算操作ax，二元运算操作ax和栈顶。
- 寄存器：
    - PC/IP，Program Counter/Instruction Pointer，即程序计数器或者叫指令指针，指向当前运行的指令。后续统一称PC。
    - SP，Stack Pointer，栈顶指针。
    - BP，Base Pointer，上一个栈的栈顶指针，函数调用时需要切换栈，返回时需要切回来就会用到。LEA相对寻址的基址。
    - ax，通用寄存器，存储操作数。
- 内存空间：
    - 代码段（code）：存储指令序列，PC指向当前运行的指令地址。
    - 数据段（data）：存储静态数据、字面量等。
    - 运行时栈（stack）：局部变量、函数调用等。
    - 没有堆（heap）：动态内存通过作弊的方式实现（native-call）。
    - 和一个C程序的内存空间（kernel、stack、heap、data、code）很像。
- 指令集：
    - Save & Load类：内存到寄存器，寄存器到内存。
    - 运算类：算术（arithmetic）运算、位（bit-wise）运算、逻辑（logical）运算。
    - 分支跳转类：branch、jump、cmp、ret等。
    - native-call：处理IO（printf/open/write等），动态内存（malloc/free/memset等）。这里是为了实现实现方便加上这些，属于作弊的手段。
- 细节：
    - 栈从高地址向低地址生长，单位为4字节，压栈*--sp = xxx。
    - 32位内存地址，用int即可保存指针。
    - 变长指令，指令长度为4字节长的指令（只是为了方便，其实不需要这么长），加上每一个操作数4字节。
    - PC单位就是4字节，PC+1用来提取操作数或者取下一条指令。
    - 其实也就是说无论是栈还是code段用一个int数组即可轻松实现。如何改成64位：最简单的方法就是将PC、寄存器、栈单位改成8个字节。


### 指令集细节

Save & Load：内存与寄存器之间的数据流通，IMM和LEA是**单操作数**指令，其他都没有操作数。
- IMM：加载立即数到寄存器ax。
- LEA：load effective address（x86经典指令，这里含义有一些区别），取立即数（相对地址）加到bp上然后将得到的地址（绝对地址）保存到ax中。
- LC：load char to ax。
- LI：load int to ax。
- SC：save char to stack top。
- SI：save int to stack top。
- PUSH：寄存器ax压到栈顶。

运算类：指令**无操作数**，运算的两个操作数是栈顶和ax，结果保存在ax。
- 算术运算：ADD、SUB、MUL、DIV、MOD，即加减乘除取模。
- 位运算：AND、OR、XOR、SHL、SHR，即与、或、异或、左移、右移。没有NOT，为了简化非运算直接在parse过程中实现了。
- 逻辑运算：EQ、NE、LT、LE、GT、GE，=、!=、<、<=、>、>=。

分支跳转类：**单操作数**。
- JMP：无条件跳转，PC to XXX。
- JZ：有条件跳转，寄存器为0时跳转。
- JNZ：有条件挑战，寄存器不为0时跳转。

函数调用相关的跳转：
- CALL：CALL addr，调用addr地址的函数。
- NVAR：New stack frame for variables，给变量新建栈帧。操作数是变量个数，比如NVAR 4。应该位于一个函数开头，在栈上为函数参数新建存储空间。在这一步做现场保护。
- DARG：Delete stack frame for arguments。一个操作数，如DARG 2，清除为参数分配的内存。
- RET：函数返回，负责恢复现场并切换到下一条指令。

native-call：直接调用C库函数。
- OPEN：打开文件。
- CLOS：关闭文件
- READ：从设备读数据。
- WRIT：写数据到设备。
- PRTF：写到标准输出（fd=1）。
- MALC：malloc。
- FREE：free。
- MSET：memset。
- MCPY：memcpy。
- EXIT：exit，结束程序。


函数调用细节：
- 实现一个函数调用需要的东西
    - 函数地址：由parser生成，体现为CALL的操作数。
    - 参数的值：执行CALL前将参数压栈，主调函数负责，CALL执行后主调函数负责清理。
    - 函数返回值：最终保存在ax中。
    - 返回地址：CALL下一条指令。
- 函数的指令保存在什么位置：code区。
- 如何知道函数地址：编译生成VM指令时由编译器确定。
- 调用函数需要保存和恢复现场，现场有些什么东西：bp和sp。
- bp是什么：
    - 用来做相对寻址的基地址，LEA的地址是相对于bp的。
    - 每个函数都有自己的bp，在函数内的LEA用的相对地址都是相对于它自己的bp。
- 讨论实现时结合实例讨论细节。

考虑多层函数调用，最后调用的那一层一定是最先释放的，整个后进先出的关系就可以用栈来描述。用栈实现是最简单的。

一个编程语言并不一定必须要有函数调用，函数调用的存在只是为了代码复用、减少冗余。简单如[brainfuck](https://zh.wikipedia.org/wiki/Brainfuck)就没有，语法就类似于定义了一个虚拟机指令集，只需要用50行代码实现虚拟机，按顺序边parse边执行，实现一个编译器（准确来说这东西似乎不需要编译，语法就是类似于汇编指令一样）从未如此简单！




## VM实现

理解思路，不需要关注细节，细节请看源码（TODO）。

Save & Load：
```C++
if (op == IMM) ax = *pc++; // load immediate to ax
else if (op == LEA) ax = (int)(bp + *pc++); // load address to ax
else if (op == LC) ax = *(char*)ax; // load char to ax
else if (op == LI) ax = *(int*)ax; // load int to ax
else if (op == SC) *(char*)*sp++ = ax; // save char to adress that is saved in stack top, then pop.
else if (op == SI) *(int*)*sp++ = ax; // save int to address then pop.
else if (op == PUSH) *--sp = ax; // push ax to stack top
```

运算类：
```C++
if (op == ADD)          ax = *sp++ +  ax;
else if (op == SUB)     ax = *sp++ -  ax;
else if (op == MUL)     ax = *sp++ *  ax;
else if (op == DIV)     ax = *sp++ /  ax;
else if (op == MOD)     ax = *sp++ %  ax;
else if (op == AND)     ax = *sp++ &  ax;
else if (op == OR)      ax = *sp++ |  ax;
else if (op == XOR)     ax = *sp++ ^  ax;
else if (op == SHL)     ax = *sp++ << ax;
else if (op == SHR)     ax = *sp++ >> ax;
else if (op == EQ)      ax = *sp++ == ax;
else if (op == NE)      ax = *sp++ != ax;
else if (op == LT)      ax = *sp++ >  ax;
else if (op == LE)      ax = *sp++ <  ax;
else if (op == GT)      ax = *sp++ >  ax;
else if (op == GE)      ax = *sp++ >= ax;
```

分支跳转：**重点**。

```C++
if (op == JMP)          pc = (int*)*pc; // jump to 
else if (op == JZ)      pc = ax ? pc+1 : (int*)*pc; // jump if ax is zero.
else if (op == JNZ)     pc = ax ? (int*)*pc ? pc+1; // jump if ax is not zero.
```

函数调用：
```C++
// call function: push pc+1 to stack, jump to function addr
if (op == CALL) {*--sp = (int)(pc+1); pc = (int*)*pc;}
// new stack frame for local variables: save bp to caller stack, sp become new bp, allocate local variables space
if (op == NVAR) {*--sp = (int)bp; bp = sp; sp = sp - *pc++;}
// delete stack frame for arguments
if (op == DARG) sp = sp + *pc++;
// return: reverse of NVAR, jump to next instruction
if (op == RET) {sp = bp; bp = (int*)*sp++; pc = (int*)sp++;}
```

函数调用实例：
```C++
int add(int a, int b)
{
    return a + b;
}
int main()
{
    int c = add(3, 4);
    printf("%d", c);
    return 0;
}
```

生成的指令：
```
// add function
NVAR 1  // new stack frame for local variables(return value) in add function
LEA -1  // load addr of return value to ax
PUSH    // push addr to stack
LEA 3   // load addr of first argument
LI      // load first argument to ax
PUSH    // push first argument to stack top
LEA 2   // load addr of second argument
LI      // load second argument to ax
ADD     // get add result in ax
SI      // save return value to the addr of NVAR allocate, then pop.
LEA -1  // load addr of return value to ax
LI      // load return value to ax
RET     // recover sp and bp, free all memory that this function call allocate.

// main function
...
IMM 3   // prepare first argument
PUSH
IMM 4   // prepare second argument
PUSH
call add_function_addr
DARG 2
...

// state of stack after NVAR
...(stack of main)                                     relative address to bp
3                                                                     3
4                                                                     2
addr of DARG(pushed by CALL)
bp of main(pushed by NVAR) <----------- current bp
location for return value <------------ current sp                   -1
invalid ...
```

整个过程描述；
- 由主调函数负责调用前分配实参的空间和调用后的清理。
- CALL跳转到相应函数地址之前先将下一条指令地址压栈。
- 由NVAR负责保存现场。所以说一个函数调用必须要有NVAR？就算函数中没有变量声明，返回值为空也要有。NVAR必须位于函数的开始。保存现场的过程；
    - 将bp压栈保存。
    - sp变成新的bp。
    - 分配函数内局部变量的内存。
- 由RET负责恢复现场：
    - 恢复sp为bp，函数中的局部变量内存就全部废弃了。
    - 从栈中恢复bp。
    - pc切换到函数后下一条指令。
- 下一条指令就是DARG负责清理参数空间。此时data区恢复到和函数调用前完全一致，函数调用返回值保存在ax中。
- 细节：
    - LEA取参数时，地址是相对bp，也就是CALL时的sp。相对地址是负的，因为参数由主调函数负责分配，在当前bp上面。参数个数确定，每个参数的相对地址就是确定的。LEA 3和LEA 2中的3和2是由编译器确定的。
    - 取函数内局部变量时，相对地址是正的，因为局部变量是在函数体内分配的。

native-call：为了完成自举所以需要，直接调用本地C语言库函数。
```C++
// from c4: https://github.com/rswier/c4/blob/master/c4.c
else if (i == OPEN) ax = open((char *)sp[1], *sp);
else if (i == READ) ax = read(sp[2], (char *)sp[1], *sp);
else if (i == CLOS) ax = close(*sp);
else if (i == PRTF) { t = sp + pc[1]; a = printf((char *)t[-1], t[-2], t[-3], t[-4],t[-5], t[-6]); }
else if (i == MALC) ax = (int)malloc(*sp);
else if (i == FREE) free((void *)*sp);
else if (i == MSET) ax = (int)memset((char *)sp[2], sp[1], *sp);
else if (i == MCMP) ax = memcmp((char *)sp[2], (char *)sp[1], *sp);
else if (i == EXIT) { printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; }
// write
else if (i == WRIT) ax = write(sp[2], (char *)sp[1], *sp);
```

## 编译器实现

## 问题

- 局部变量必须要声明在函数开始。

## TODO

- parser & Code Generator
- GC for malloc and free
- switch ?
- break & continue ?
- #include & typedef ?
- multi-file support ?