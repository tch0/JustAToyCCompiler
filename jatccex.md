<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [扩展c4](#%E6%89%A9%E5%B1%95c4)
  - [控制流](#%E6%8E%A7%E5%88%B6%E6%B5%81)
    - [for](#for)
    - [do while](#do-while)
    - [switch](#switch)
    - [break](#break)
    - [continue](#continue)
    - [goto & label](#goto--label)
  - [自定义类型支持](#%E8%87%AA%E5%AE%9A%E4%B9%89%E7%B1%BB%E5%9E%8B%E6%94%AF%E6%8C%81)
  - [enum](#enum)
  - [union & struct](#union--struct)
    - [文法](#%E6%96%87%E6%B3%95)
    - [.和->运算符](#%E5%92%8C-%E8%BF%90%E7%AE%97%E7%AC%A6)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# 扩展c4

- 控制流：`for` `do while` `switch` `break` `continue` `goto`
- 命名`enum`支持，简单等同于`int`
- `union` `struct`类型支持，相关操作符`. ->`支持
- 使用了支持的新特性替换了部分实现细节
- 同样自举


## 控制流

### for

```C++
for (init; condition; iter)
{
    for_statements;
}
```
文法：
```EBNF
for_statement = for, "(", [expression], ";", [expression], ";", [expression], ")", statement;
```

生成代码：
```
[init]
[condition]/IMM 1       <------ [a] // has condition/no condition
JNZ [c]
JMP [end]
[iter]                  <------ [b]
JMP [a]
[for_statements]        <------ [c]
JMP [b]
...                     <------ [end]
```
for循环比较特殊，条件可能是空的，需要做一个判断。

因为语法分析加生成代码是线性的过程，如果循环体生成完了再回去生成for的迭代表示的话就需要回溯。所以这里直接生成，但用JMP改变一下执行顺序即可。不怎么优雅，但有用。

### do while

```C++
do
{
    do_while_statements;
} while(condition);
```
文法：
```EBNF
do_while_statement = do, statement, while, "(", [expression], ")", ";";
```

生成代码：
```
[do_while_statements]       <------ [a]
[condition]
JNZ [a]
...
```

### switch

要生成跳表，不是那么简单，TODO。

### break

```C++
while (condition)
{
    xxx;
    break;
}

for (init; condition; iter)
{
    xxx;
    break;
}

do
{
    xxx;
    break;
} while(condition);
```

文法：
```EBNF
break_statement = break, ";";
```

先实现循环中的break，switch暂未实现，实现时再考虑switch中的break。

循环中的break的作用是结束循环，跳转到循环后的第一条语句执行。

生成代码：
```
JMP [end]
```
需要记录外层循环的结束点，直接JMP就行，解析到break时还没有结束循环的解析，所以需要记录所有保存break对应的JMP指令的跳转地址的code段地址。因为循环可以嵌套，所以只用一个全局的还不够，还需要能够唯一地标识一个循环，这里就选用循环开始的代码段地址好了。为了方便直接存在同一个列表中，而不是每个循环存一个列表，那么就需要两个域，一个循环开始地址，一个保存用来保存break跳转地址的code段地址。continue同理。

这个循环地址对于`while` `do while` `for`都选择上述生成代码中的地址`[a]`。

还是定义一个宏，后续实现`struct`后会改用`struct`：
```C++
enum Break_continue_list_domain { Loop = 0, BCAddress, ListSize };
```

### continue

```C++
while (condition)
{
    xxx;
    continue;
}

for (init; condition; iter)
{
    xxx;
    continue;
}

do
{
    continue;
} while(condition);
```

文法：
```EBNF
continue_statement = continue, ";";
```

`continue`语句的作用是结束此轮循环，跳转到条件开始执行。

生成代码：

```
JMP [entry]
```

在`while`和`do while`应该是条件判断的位置，分别位于开始和末尾，`for`中则是`final`语句位置。

### goto & label

考虑到变量定义都被集中到函数开头了，`goto`的实现应该会简单许多。

```C++
goto lebel;
```

文法：
```EBNF
goto_statement = goto, id;
label_statement = id, ":";
```

生成代码：
```
JMP [addr_of_label]
```

细节：
- 任何语句（但非声明）可以前附任意数量的标号，每个都声明一个 标识符 为标号名，标号名必须在闭合的函数中唯一。
- 标号声明自身没有效果，不会以任何方式变更控制流，或修改跟随其后的语句的行为。
- 标号应当后随语句。
- 标号可以不带其后随语句出现。若标号单独出现在块中，则表现为如同它后随一条空语句。

## 自定义类型支持

自定义类型包括命名enum，union，struct，为了支持这三个特性，就需要扩展现在的类型系统，支持使用标识符来表示类型。

具体做法：

1. 在`Identifier_type`枚举中新增枚举值`EnumType UnionType StructType`分别用以标识用户定义的类型。用在符号表记录的`Class`域。
```C++
enum Identifier_type
{
    EnumVal,    // enum value as constant
    Fun,        // function
    Sys,        // system call (native-call)
    Glo,        // global variables
    Loc,        // local variables
    EnumType,   // user defined enum type
    UnionType,  // user defined union type
    StructType  // user defined struct type
};
```
2. 除此之外，还需要一个唯一的值来区分具体类型是什么。在`Var_type`枚举中添加枚举值，用在符号表记录的`Type`域中。
```C++
enum Var_type { CHAR = 0, INT, ENUM, UNION = 100, STRUCT = 500, PTR = 1000 };
```
其中使用`UNION` ~ `STRUCT-1`的值来表示联合类型，用`STRUCT` ~ `PTR - 1`来表示结构体类型，即是支持400中自定义`union`，500种自定义`struct`，`enum`的话直接解释为`int`，所有都一样。应该够用了，不够用了改一下枚举值即可。

3. 还需要修改所有进行了类型检查的地方，以支持新增的类型。
4. 为了支持union和struct，还需要特定的结构来存储成员的类型和名称等信息，这就需要新的结构了，独立于现在的符号表。它仍然是符号表的一部分。用于符号表项的`Value`域。具体实现后续再具体研究。
5. 需要考虑类型和变量冲突，作用域覆盖等细节，直接规定已经被定义的自定义类型不能声明同名变量和函数以规避这个问题。函数名感觉也应该这样做。同时已经被定义为全局变量也不能再被定义同名类型或者函数。

## enum

简单起见，解析到定义了命名enum类型的变量时，它的类型将成为int。相当于为定义了一个别名。

## union & struct

### 文法

### .和->运算符