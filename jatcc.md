<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [编译器](#%E7%BC%96%E8%AF%91%E5%99%A8)
- [编译器设计](#%E7%BC%96%E8%AF%91%E5%99%A8%E8%AE%BE%E8%AE%A1)
- [VM设计](#vm%E8%AE%BE%E8%AE%A1)
  - [总览](#%E6%80%BB%E8%A7%88)
  - [指令集细节](#%E6%8C%87%E4%BB%A4%E9%9B%86%E7%BB%86%E8%8A%82)
  - [VM实现](#vm%E5%AE%9E%E7%8E%B0)
- [编译器实现](#%E7%BC%96%E8%AF%91%E5%99%A8%E5%AE%9E%E7%8E%B0)
  - [标准流程](#%E6%A0%87%E5%87%86%E6%B5%81%E7%A8%8B)
  - [词法分析](#%E8%AF%8D%E6%B3%95%E5%88%86%E6%9E%90)
  - [语法分析](#%E8%AF%AD%E6%B3%95%E5%88%86%E6%9E%90)
  - [语义分析与符号表](#%E8%AF%AD%E4%B9%89%E5%88%86%E6%9E%90%E4%B8%8E%E7%AC%A6%E5%8F%B7%E8%A1%A8)
  - [递归下降](#%E9%80%92%E5%BD%92%E4%B8%8B%E9%99%8D)
  - [运算符优先级分析](#%E8%BF%90%E7%AE%97%E7%AC%A6%E4%BC%98%E5%85%88%E7%BA%A7%E5%88%86%E6%9E%90)
  - [代码生成](#%E4%BB%A3%E7%A0%81%E7%94%9F%E6%88%90)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

## 编译器

先看一下Wiki：
>将便于人编写、阅读、维护的高级计算机语言所写作的源代码程序，翻译为计算机能解读、运行的低阶机器语言的程序，也就是可执行文件。编译器将原始程序（source program）作为输入，翻译产生使用目标语言（target language）的等价程序。源代码一般为高级语言（High-level language），如Pascal、C、C++、C# 、Java等，而目标语言则是汇编语言或目标机器的目标代码（Object code），有时也称作机器代码（Machine code）。
一个现代编译器的工作流程：源代码（source code）→ 预处理器（preprocessor）→ 编译器（compiler）→ 汇编程序（assembler）→ 目标代码（object code）→ 链接器（linker）→ 可执行文件（executables）。

一个工业级别的编译器的编译过程：
```
Front End -> Optimizer -> Back End
```
不同的编译器有不同的设计，只是概念性的阶段。

例子：
```
        Front End              Optimizer              Back End
x = a+b ------------->   AST  -------------->  IL -------------------> machine code(assembly)
                        =(Assign)    CSE        Instruction Selection     load a to r1
      1.Scanning        / \          DCE        Instruction Scheduling    load b to r2
      (lexing)         x    +        ...        Register Allocation       add r1,r2 to r3
      2.Parsing            / \                                            store r3 to x
      3.Sematic Analysis  a   b

high level --------------->  intermediate language  ------------------->   low level
```

前端：
- **词法分析**（Scanning/Lexing）：将源码切分为记号（token/lexeme）序列。类似于自然语言中的词汇。
- **语法分析**（Parsing/Syntactic Analysis）：解析语句(statement)。类似于自然语句中的句子，识别标识符表达式是否合法等。比如`x = a+b*/;`在语法上就是非法的。语法分析可以本地检查，也就是上下文无关，拿到一条语句就可以检查它的语法是否合法。
- 使用文法（Grammar）定义合法的语句。
- **语义分析**（Semantic Analysis）：语法上合法的语句可能语义上没有意义。语义是上下文相关的，比如表达式必须需要其中的变量有定义，且能够进行表达式中的运算。上下文存放在符号表中，解析到变量声明时就会在符号表中创建一条新的记录，当用到时就需要从符号表中查找它的含义。比如将一个指针进行除法就是没有意义的`char* p; int x; x = p / 10;`。
- 前端生成结果一般是AST或者其他的线性的中间代码（IR）。

中间优化：
- 理想情况下中间优化过程是独立于机器的，搭载不同的后端就可以生成不同机器、处理器的机器码。典型优化比如CSE,DCE等。
- Common Subexpression Elimination(CSE)：公共子表达式消除。如`x = a+b+c; y = d/(a+b);`其中的`a+b`就可以不用重复计算，将其等价于`t = a+b; x = t+c; y = d/t;`。考虑到可能多了从内存加载数据的过程、缓存命中情况的可能变化、增加了寄存器占用（寄存器数量是有限的，这可能会降低性能）等多重影响，也许重复计算还会更快，不存在100%的保证。
- 代码优化并不会100%保证代码变快，考虑到缓存寄存器分配等因素，一般来说优化也并不会100%地独立于机器，更符合实际的说法是等价的代码转换（transformation）而不是优化（optimization），虽然很多时候确实能够得到优化。
- DCE(Dead Code Elimination)：死代码剔除，剔除不可能执行到的代码，比如`if(0) {}`这种。

后端：
- 从中间表示生成机器码。
- 最后得到的机器码一般不包含符号信息，变量都是用内存地址表示。但在实践中普遍会为了一些目的在整个编译的阶段保留符号信息，典型的就是调试。
- 步骤：[指令选择](https://en.wikipedia.org/wiki/Instruction_selection)、[指令调度](https://en.wikipedia.org/wiki/Instruction_scheduling)、[寄存器分配](https://en.wikipedia.org/wiki/Register_allocation)。
- **指令选择**(Instruction Selection))：为中间表示（IR）选择更高效的指令序列。输出更low-level的很像目标机器语言的中间表示。在这个阶段中将使用虚拟寄存器而不是真实的寄存器。
- **指令调度**(Instruction Scheduling)：通过**加强指令级并行**(More ILP, Instruction-level parallelism)，使程序在指令流水线（处理器的多级流水线）上更高效地运行。当然是在不改变程序运行结果的前提下。
    - **重组指令顺序**，减少或阻止流水线停顿（主要由结构冒险（受限的处理器资源）、数据冒险（数据依赖导致读写数据的顺序发生错误，比如需要数据去读取时还没有将应该的结果写进去）、控制冒险（分支指令控制流不能流水开始时就判断出分支结果）导致）。CPU设计和编译优化都需要考虑冒险。
    - 在运行一条指令时同时运行不依赖于它的其他指令，比如在很慢的Load类指令操作的同时做其他不依赖的计算操作。同时并不是在同一个时钟周期同时开始执行多条指令，而是尽可能在两个相邻时钟周期开始执行并且尽可能减少流水线停顿，毕竟处理器是流水线执行的，同一时刻大概率正在执行的多条指令的不同阶段。为了实现这类操作就需要维护指令间的依赖关系（也就是**依赖图**，一个有向无环图），最终这个图任何一个**拓扑排序**都是有效的指令调度表。编译器通常会结合指令的**预计执行时钟周期**排列出一个更优的执行序列。某些时候这种对指令执行时钟周期数的预计只有在**缓存命中**（编译器可能假定缓存会命中）时才有意义，否则可能整个过程块的指令调度意义都不大，指令调度也不能保证100%会有效率提升。编译时的信息和运行时的信息不可能100%吻合，很多因素都会影响代码的实际运行效率。
    - 指令调度一般在寄存器分配之前，也可以在之后。在寄存器配置前可以最大化程序的并行度，但可能会增加寄存器需求（比较典型的是将load指令提到前面来统一执行就会这样），而寄存器溢出又会导致性能下降。如果在寄存器配置之后，指令排序又可能会因为寄存器配置产生的假依赖而受到一定限制。通常，更高的指令级并行度就意味着需要更多的寄存器，很多优化将取决于目标机器的架构，指令调度是一个[NP完全问题](https://zh.wikipedia.org/wiki/NP%E5%AE%8C%E5%85%A8)。实践中不可能用指数级或者阶乘级复杂度的算法，所以只能说大部分情况能够得到优化，并不保证一定最优，甚至某些情况可能负优化。
    - 支持**乱序执行**的处理器（如Intel X86）会在运行时做指令重排，在这种情况下可以更加关注于减少寄存器的需求、减少load和store指令的数量。一般来说这种乱序执行有一定的硬件缓冲限制（reorder buffer），所以只能看到一部分指令，不像编译器能够看到整个程序。编译器层面的指令调度同样有意义。另外分支跳转的存在，也可能会限制处理器层面的乱序执行的发挥。
- **寄存器分配**（Register Allocation）：在寄存器分配前，编译器假定有无限的寄存器，并使用虚拟的寄存器，在寄存器分配中才将虚拟寄存器映射到实际的有限的物理寄存器。寄存器中一般存放局部或者临时变量。典型情况是物理寄存器比虚拟寄存器少，例如考虑一个块作用域`{}`中的局部变量，维护物理寄存器的生命周期，超过生命周期就可以重新分配不再使用的寄存器给新的变量。如果寄存器不够用就需要将一部分寄存器中的值存到内存（栈）中，需要时再加载进来（添加的指令称为Spin Code），毫无疑问这将会影响执行效率。换言之程序的寄存器数量要求越高，执行效率就会越低。寄存器分配致力于减少内存和寄存器的交换。【效率参考：命中L1缓存大概2~4个时钟周期、从寄存器取值做计算可以忽略、缓存未命中从内存中读数据可能需要几百个时钟周期，可能会两个数量级差距】。
- 综上，编译优化是平衡和取舍的过程，一个方面变好了另一个方面可能会变差，没有编译器能任何时候都生成完美的代码。

## 编译器设计

- 运行在极简的虚拟机上。
- 独立的词法分析，语法语义分析和代码生成合为一体，没有中间优化过程，代码生成过程中为特定的结构生成特定的指令，没有也不需要指令调度和寄存器分配。
- 语法分析过程中变量、函数定义部分使用递归下降进行处理，表达式采用优先级分析法进行处理。

## VM设计

### 总览

极简设计的虚拟机：
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
    - 如果指令有操作数，那么实现指令时就需要将取出操作数并移到下一条指令开头，基本操作就是*pc++。
    - 也就是说无论是栈还是code段用一个int数组即可轻松实现。如何改成64位：最简单的方法就是将PC、寄存器、栈单位改成8个字节，c4的实现中的`#define int long long`。


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
- JNZ：有条件跳转，寄存器不为0时跳转。

函数跳转相关指令：
- JSR：jump subroutine，跳转到子程序，操作数为子程序的地址。
- ENT：enter subroutine，进入子程序，放在子程序开头，切换栈帧，并为子程序内的局部变量分配内存，操作数为内存单元数量。
- ADJ：adjust stack，调整栈指针sp，比如用于快速释放主调函数为参数分配的栈内存，操作数是栈帧数量。
- LEV：leave subroutine，退出子程序，放在子程序的末尾，将栈帧切换回主调函数，释放子程序中的内存。


native-call：直接调用C库函数。
- OPEN：打开文件。
- CLOS：关闭文件。
- READ：从设备读数据。
- WRIT：写数据到设备。
- PRTF：写到标准输出（fd=1）。
- MALC：malloc。
- FREE：free。
- MSET：memset。
- MCMP：memcmp。
- EXIT：exit，结束程序。


函数调用细节：
- 实现一个函数调用需要的东西
    - 函数地址：由parser生成，体现为JSR的操作数。
    - 参数的值：执行JSR前将参数压栈，主调函数负责，JSR执行后主调函数负责清理。
    - 函数返回值：最终保存在ax中。
    - 返回地址：JSR下一条指令。
- 函数的指令保存在什么位置：code区。
- 如何知道函数地址：编译生成VM指令时由编译器确定。
- 调用函数需要保存和恢复现场，现场有些什么东西：bp和sp。
- bp是什么：
    - 用来做相对寻址的基地址，LEA的地址是相对于bp的。
    - 每个函数都有自己的bp，在函数内的LEA用的相对地址都是相对于它自己的bp。
- 讨论实现时结合实例讨论细节。

考虑多层函数调用，最后调用的那一层一定是最先释放的，整个后进先出的关系就可以用栈来描述。用栈实现是最简单的。

一个编程语言并不一定必须要有函数调用，函数调用的存在只是为了代码复用、减少冗余。

### VM实现

理解思路，不需要关注细节，细节请看源码。

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
if (op == JSR) {*--sp = (int)(pc+1); pc = (int*)*pc;}
// new stack frame for local variables: save bp to caller stack, sp become new bp, allocate local variables space
if (op == ENT) {*--sp = (int)bp; bp = sp; sp = sp - *pc++;}
// delete stack frame for arguments
if (op == ADJ) sp = sp + *pc++;
// return: reverse of ENT, and jump to next instruction
if (op == LEV) {sp = bp; bp = (int*)*sp++; pc = (int*)sp++;}
```

函数调用实例：
```C++
int add(int a, int b)
{
    int res;
    res = a + b;
    return res;
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
ENT 1  // new stack frame for local variables(return value) in add function
LEA -1  // load addr of return value to ax
PUSH    // push addr to stack
LEA 3   // load addr of first argument
LI      // load first argument to ax
PUSH    // push first argument to stack top
LEA 2   // load addr of second argument
LI      // load second argument to ax
ADD     // get add result in ax
SI      // save return value to the addr of ENT allocate, then pop.
LEA -1  // load addr of return value to ax
LI      // load return value to ax
LEV     // recover sp and bp, free all memory that this function call allocate.

// main function
...
IMM 3   // prepare first argument
PUSH
IMM 4   // prepare second argument
PUSH
JSR add_function_addr
ADJ 2
...

// state of stack after ENT
...(stack of main)                                     relative address to bp
3                                                                     3
4                                                                     2
addr of ADJ(pushed by JSR)
bp of main(pushed by ENT) <----------- current bp
location for local variable  <-------- current sp                    -1
invalid ...
```

整个过程描述；
- 由主调函数负责调用前分配实参的空间和调用后的清理。
- JSR跳转到相应函数地址之前先将下一条指令地址压栈。
- 由ENT负责保存现场。函数（子程序）必须由ENT开始，如果函数中没有变量声明就是ENT 0。ENT指令保存现场的过程：
    - 将bp压栈保存。
    - sp变成新的bp。
    - 分配函数内局部变量的内存。
- 由LEV负责恢复现场：
    - 恢复sp为bp，函数中的局部变量内存就全部废弃了。
    - 从栈中恢复主调函数bp。
    - pc切换到函数调用下一条指令。
- 下一条指令如果有参数的话就是ADJ，负责清理参数空间。然后栈状态会恢复到和函数调用前完全一致，函数调用返回值保存在ax中。
- 细节：
    - LEA取参数时，地址是相对bp，也就是JSR时的sp。相对地址是负的，因为参数由主调函数负责分配，在当前bp上面。参数个数确定，每个参数的相对地址就是确定的。LEA 3和LEA 2中的3和2是由编译器确定的。
    - 取函数内局部变量时，相对地址是正的，因为局部变量是在函数体内分配的。
    - 更准确地说，函数参数的相对地址是：...,4,3,2。局部变量的相对地址是：-1,-2,-3,...。

native-call：为了完成自举所以需要，直接调用本地C语言库函数。
```C++
// from c4: https://github.com/rswier/c4/blob/master/c4.c
else if (op == OPEN) ax = open((char *)sp[1], *sp);
else if (op == READ) ax = read(sp[2], (char *)sp[1], *sp);
else if (op == CLOS) ax = close(*sp);
else if (op == PRTF) { t = sp + pc[1]; ax = printf((char *)t[-1], t[-2], t[-3], t[-4],t[-5], t[-6]); }
else if (op == MALC) ax = (int)malloc(*sp);
else if (op == FREE) free((void *)*sp);
else if (op == MSET) ax = (int)memset((char *)sp[2], sp[1], *sp);
else if (op == MCMP) ax = memcmp((char *)sp[2], (char *)sp[1], *sp);
else if (op == EXIT) { printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; }
// write
else if (op == WRIT) ax = write(sp[2], (char *)sp[1], *sp);
```

## 编译器实现

### 标准流程

通常来说，编译器的编译流程是：
- 词法分析 --[token流]-> 语法分析 --[AST]-> 语义分析 --[标注过的AST+符号表]-> 代码生成 -> [目标代码]。
- 每一个过程称为一趟，每一趟的输出作为下一流程的输入，而不必在源代码上再做遍历。
- 在实际实现中词法分析通常和语法分析混合在同一趟中来做，词法分析器生成token流，语法分析器使用token流以驱动词法分析器，从词法分析器中拉（pull）出一个个token，经过语法分析后生成抽象语法树。
- 语义分析则是检查AST中的内容，检查语法是否有问题，并做一些标记。比如在遇到了赋值语句如a=2，就需要检查符号a是否存在，这是就需要符号表。在语义分析过程中建立符号表。

单趟的编译过程将输入的源代码经过 词法分析、语法分析、语义分析、代码生成 得到目标虚拟机代码。由于把各种过程混合到语法分析过程中，代码往往高度浓缩而难懂，例如c4。

当然也可以不生成目标虚拟机代码，乃至于直接把解释器也整合到语法分析中。比较正常的、考虑执行效率的话一般不会这么极端。

### 词法分析

需要识别的东西：
- 关键字
    - `void` `char` `int` `enum` `if` `else` `while` `return`
    - 基本内置类型`int` `char`。
    - 基本类型的复合类型`int*` `char**`。
    - system call这里作为关键字来解析，或者说是标识符。
- 标识符：变量、函数名
- 字面量：整数字面量，字符串字面量
- 操作符：`= ?: || && | ^ & == != < > <= >= << >> + - * / % ++ -- [] () {}`
- 可忽略的字符：注释，space，tab，CR LF等空白符，非ASCII编码。

这个系列已经说得非常详细了，就不赘述了。[手把手教你构建 C 语言编译器（3）- 词法分析器](https://lotabout.me/2015/write-a-C-interpreter-3/)。

### 语法分析

语法分析以词法分析的token stream作为输入，按照语言的文法检查语句的合法性。

语法分析设计：
- 自顶向下实现。
- 单目运算符、函数调用、变量声明等部分使用[递归下降](https://en.wikipedia.org/wiki/Recursive_descent_parser)（recursive-descent）。
- 表达式采用[运算符优先级](https://en.wikipedia.org/wiki/Operator-precedence_parser)（operator precedence）分析方式。

其中：
- 递归下降就是根据文法相互递归，实现难度不高，在准确地实现文法的同时还需要完善地识别语法错误。
- 运算符优先级分析则是使用两个栈，一个用来处理运算符优先级，一个用来存放表达式的中间计算结果。处理运算符优先级的栈通过函数递归调用隐含在函数调用栈里，存放中间表达式的栈隐含在了运行在基于栈的虚拟机上的指令中。

两者都足以分析整个程序的语法，但前者在分析具有多级优先级的表达式时比较慢，后者在分析声明、语句等一般不看做运算符的结构时代码不直观。所以一般结合使用互补优势。

### 语义分析与符号表

语法分析是通过解析源码是否符合规定的文法来实现的，也就是上下文无关的。而语义分析则需要上下文的信息，这些信息就保存在**符号表**中。

因为没有支持struct，所以实现中用了一个动态数组来模拟结构体，作为一条符号表记录。

一条符号表记录中的内容：
- Token: 标记，值应该是Token_type类型的。
- Hash: 根据名称计算出的一个哈希值，加速查找，不需要每次都去遍历名字比较。
- Name: 名称，计算出哈希值之后就不需要用它了，指向源文件某位置的char*指针。
- Class: 标识符的类型，Id类型才需要，值为Identifier_type中枚举。
- Type: 标识符的变量类型或者函数返回值类型，值为Var_type枚举中普通类型与PTR组合得到的值。
- Value: 标识符的值：如果标识符是函数，则是函数地址，如果是变量或者字符串常量就是地址，如果是字面量则是具体的值。
- GClass/GType/GVlaue: 同Class/Type/Value，处理全局作用域对函数作用域的覆盖。
- IdSize: struct长度。

有了符号表，才能在语义分析时做上下文相关的分析。由于这里语法分析、语义分析、代码生成是一起做的，所以符号表的使用贯穿在整个扫描解析过程中。

关于符号表中的主要操作细节有；
- 在未开始解析前将关键字扫描到符号表中，主要是对名称做哈希，方便后续遇到更好识别出token。
- 系统调用同样会这样做，不过会被识别为系统调用，会保存返回值类型和对应的指令方便生成虚拟机指令。
- void也被添加到符号表，并将token类型转换为char对应类型，即将void完全等同于char看待。
- 需要特地记录main函数的符号表记录，方便以main函数作为入口开始执行程序。
- 在词法分析中就遇到一个标识符会去符号表中查找是否已经被定义，如果没有则会创建新的记录。
- 枚举值作为常量被添加到符号表。
- 在处理局部变量对全局变量的作用域覆盖时，符号表发挥作用，进入函数后，会将函数内定义的与全局变量同名的局部变量保存起来，离开函数时恢复。【标准C语言是块作用域，需要实现嵌套符号表，这里简化了仅支持全局和函数两个作用域】。
- 局部变量将使用相对bp地址来记录，生成代码使用LEA指令取到地址，全局变量则使用绝对地址。

### 递归下降

传统上，编写语法分析器有两种方法，一种是自顶向下，一种是自底向上。自顶向下是从起始开始符开始，不断地对非终结符进行分解，直到匹配输入的终结符；自底向上是不断地将终结符进行合并，直到合并成起始的非终结符。

其中的自顶向下方法就是我们所说的递归下降。

这里实现的C语言子集使用EBNF文法描述：
```EBNF
program = {global_decl};
global_decl = enum_decl | func_decl | var_decl;
enum_decl = enum, [id], "{", id, ["=", number], {",", id, ["=", number]} ,"}";
func_decl = ret_type, id, "(", param_decl, ")", "{", func_body, "}";
param_decl = type, {"*"}, id, {",", type {"*"}, id};
ret_type = void | type, {"*"};
type = int | char;
func_body = {var_decl}, {statement};
var_decl = type {"*"}, id, {",", id}, ";";
statement = if_statement
        | while_statement
        | "{", {statement}, "}"
        | return, [expression], ";"
        | [expression], ";";
if_statement = if, "(", expression, ")", statement, [else, statement];
while_statement = while, "(", expression, ")", statement;
```
其中`{}`表0或者多个，`[]`表可选即0或1个。`id` `number` `void` `char` `int` `enum` `if` `else` `while` `return`都是词法分析的结果最基本的token。`expression`则通过优先级分析法分析，如果使用递归下降的话由于运算符多层优先级会异常复杂。

更多关于如何实现递归下降和BNF相关文法内容可以查看：[BNF与递归下降](https://github.com/tch0/notes/blob/master/BNF&RecursiveDescent.md)。

### 运算符优先级分析

表达式将会使用运算符优先级分析法进行分析。关于优先级分析的更多细节都已经在这里了：[运算符优先级分析法](https://github.com/tch0/notes/blob/master/OperatorPrecedenceParser.md)，以及代码注释。

总体来说就是递归向右计算优先级更高的表达式后再回来计算当前运算符。
- 需要特别注意**运算符结合性**的影响：右结合的运算符应该向右计算等于或者高于当前运算符优先级的所有运算符，而左结合只需要计算高于的。
- 因为要区分不同运算符，必须使用不同枚举值，用来代指更高优先级的运算符应该选用高一级的同类优先级运算符中枚举值最小的那一个。
- 实现中由C语言文法就确定了前置的一元运算符只能是右结合的，并且是同一优先级。后置一元和绝大多数二元运算符都是左结合的。唯一右结合的只有赋值类的运算符，仅支持了`=`，所以可以用`a = b = 2;`这样用因为是右结合的。

一些实现时的小细节：
- 逗号表达式优先级最低。
- `expr ? a : b`三目运算符中间的a相当于加了括号，`expr`和`b`则不然。
- 函数调用`f(a, b, c)`中不应当支持逗号运算符，应该从赋值号一级开始解析。

### 代码生成

函数调用的调用方（前面已经分析过）：
```
...
[intructions that push all paramters into stack in order]
JSR [address of function]
ADJ [count of paramters]
...
```
函数定义：
```
ENT [count of local variables]
...
(function body)
...
[save return value to ax if need]
LEV
```
if-else结构：
```C++
if (condition)
{
    true_statements;
}

if (condition)
{
    true_statements;
}
else
{
    false_statements;
}
```
生成代码：
```
[condition]
JZ [end]
[true_statements]
...                 <-----[end]

[condition]
JZ [a]
[true_statements]
JMP [end]
[false_statements]   <-----[a]
...                  <-----[end]
```
while结构：
```C++
while (condition)
{
    while_statements;
}
```
生成代码：
```
[condition]         <-----[a]
JZ [end]
[while_statements]
JMP [a]
...                 <-----[end]
```
return语句：
```
...
[calculate expression and load to ax if there is a return value]
LEV
...
```
