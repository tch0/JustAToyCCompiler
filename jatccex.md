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
    - [做法](#%E5%81%9A%E6%B3%95)
    - [影响](#%E5%BD%B1%E5%93%8D)
  - [enum](#enum)
  - [union & struct](#union--struct)
    - [文法](#%E6%96%87%E6%B3%95)
    - [.和->运算符](#%E5%92%8C-%E8%BF%90%E7%AE%97%E7%AC%A6)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# 扩展c4

- 控制流：`for` `do while` `break` `continue` `goto`
- 命名`enum`支持，简单等同于`int`
- `union` `struct`类型支持，相关操作符`. ->`支持
- 使用支持了的特性替换部分实现细节
- 同样自举

最终实现的C语言子集的文法：
```EBNF
program = {global_decl};
global_decl = enum_decl | func_decl | var_decl;
enum_decl = enum, [id], "{", id, ["=", number], {",", id, ["=", number]} ,"}";
func_decl = ret_type, id, "(", param_decl, ")", "{", func_body, "}";
param_decl = type, {"*"}, id, {",", type {"*"}, id};
ret_type = void | type, {"*"};
type = int | char | user_defined_type;
user_defined_type = (enum | union | struct), id;
func_body = {var_decl}, {statement};
var_decl = type {"*"}, id, {",", id}, ";";
statement = if_statement
        | while_statement
        | for_statement
        | do_while_statement
        | break_statement
        | continue_statement
        | "{", {statement}, "}"
        | return, [expression], ";"
        | [expression], ";";
        | id, ":", statement;
        | goto, id, ";";
if_statement = if, "(", expression, ")", statement, [else, statement];
while_statement = while, "(", expression, ")", statement;
for_statement = for, "(", [expression], ";", [expression], ";", [expression], ")", statement;
do_while_statement = do, statement, while, "(", [expression], ")", ";";
break_statement = break, ";";
continue_statement = continue, ";";
```

现在才发现原来C语言的enum/union/struct变量声明时必须要加enum/union/struct关键字。写了这么多年C++（C++中不要求）的我震惊了，才理解为什么C语言里面定义结构都要用typedef定义一个同名的类型别名。当然这里不支持typedef，就遵循标准，其实还降低了语法分析的难度。
```C++
typedef struct node 
{
    void* data;
    node* left;
    node* right;
}node;
```

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

**不实现，现有框架下无法良好支持。**

按照整数参数的值执行代码。在需要按照整数值执行多个分支中的一个或数个之处使用，switch仅支持整数，其中的值应该是整数常量整数值，比如字符字面值，枚举等。按道理来说应该要在编译期计算，但现在并没有实现这样的机制，要么就运行时计算，同样也支持变量表达式，或者只支持常量。

通常的实现switch的三种方式：
- 逐条件判断。
- 跳转表。
- 二分查找。

当然由于是线性的解析过程，所以只好选择最简单的逐条件判断。

```C++
switch (expr)
{
    case val1: xxx;
    case val2: xxx;
    case val3: xxx; break;
    ...
    default: xxx; break;
}
```

生成代码：
```C++
[expr]
PUSH
[calcualte val1]
EQ
JNZ [loc_val1]
[load expr value]  (how to save? save to where? how many times should save?)
PUSH
[calculate val2]
EQ
JNZ [loc_val2]
...
JMP [loc_default]
[val1 statements]        <------ [loc_val1]
[val2 statements]        <------ [loc_val2]
[val3 statements]        <------ [loc_val3]
JMP [end]
...
[default statements]     <------ [loc_default]
JMP [end]
...                      <------ [end]
```
判断集中在开头，跳转到对应条目，有break就跳转到end，没有就不跳转，继续执行。
- 这样做的问题是case标签表达式计算和标签下语句被割裂开了。线性的解析过程中生成的代码也是线性的，现有的生成代码已经和code指针高度绑定了，如果修改了code的值生成到其他位置的话，其中的条件跳转地址就会有问题。现在的实施框架下有点不好办了，这算是语法语义分析和代码生成高度绑定到了一起导致的问题。
- 而且因为要判断多次，需要暂存表达式expr的结果，所以需要将其压栈，每次运算时又会出栈，所以每一次比较都要压栈，但这个值保存在哪里呢？除非expr本身就是一个局部变量或者参数，否则除了栈顶栈中是没有地方保存它的。可行的方法只有按照case语句数量直接先多次PUSH将其压到栈中，确保最坏情况要比较所有case标签的值时能够取到正确的expr表达式结果。这需要向后看然后再回溯。
- 如果限定expr只能是左值的话又限制太大了一点。如果有多个寄存器就可以用一个来保存中间计算结果了。当然也可以malloc一个单元来保存，但这种作弊的手段没有多大意义就算了。
- 改动可能略大，现有框架下不好实现，主要是地址的重新定位不好做，综合考虑放弃实现这个特性。

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

[C语言的goto语句](https://zh.cppreference.com/w/c/language/goto)，考虑到变量定义都被集中到函数开头了，`goto`的实现应该会简单许多。并且这里并不支持数组，也就不支持对goto有影响的VLA。

```C++
goto lebel;
```

文法：
```EBNF
goto_statement = goto, id;
label_statement = id, ":", statement;
```

生成代码：
```
JMP [addr_of_label]
```

细节：
- goto语句必须与标号出现在同一函数。
- 任何语句（但非声明）可以前附任意数量的标号，每个都声明一个 标识符 为标号名，标号名必须在闭合的函数中唯一。
- 标号声明自身没有效果，不会以任何方式变更控制流，或修改跟随其后的语句的行为。
- 标号应当后随语句。
- 标号可以不带其后随语句出现。若标号单独出现在块中，则表现为如同它后随一条空语句。

实现：
- 标号是一个有效的标识符，具有函数作用域，离开作用域需要释放。需要添加一种标识符类型，枚举值`Label`。
- 标号不会占用任何代码空间，仅表示一个地址，用Value域保存这个地址。
- 标号后接语句。
- 在标准的C语言中，局部变量的定义会覆盖同名函数和全局变量枚举值等的定义。但标号和函数名变量名是互不冲突的。
- 由于实现的限制，因为同样需要保存在符号表中，同名的局部变量的定义将会被覆盖。出于实现方便和代码清晰考虑，这里直接禁止标号和自定义类型、局部变量、全局变量、枚举值、函数调用、关键字同名，冲突了将直接报错。
- 实现策略：
    - 标号的解析因为前面是标识符，和函数调用、变量不好区分，可以放到表达式`expression`解析过程中来解析，但标号定义并不是表达式，解析完应该立即返回，而普通表达式语句后面必须有`;`，所以使用标号就必须有分号。而且这会导致能用表达式的地方（比如if/while/for的表达式中）定义了标号都会被认为是语法正确。总体来说不是一个好的选择，也不符合语义。
    - 更好的实现方式应该是实现一个当前没有的回溯手段（比如将词法分析实现为更灵活的token流），在解析语句时先尝试解析为标号，不匹配时将解析了的token回溯到解析前状态，再尝试解析为表达式。
- 也就是必须要能够回溯或者相互递归才能够比较好地支持标号，为了更好地支持，实现一个记录回溯点和回溯到上一个记录点的功能，记录和恢复token、源码状态、行号即可。注意在中间不应该生成任何代码。
- 标号后必须有语句，在这里，它不应该位于变量声明前（标准C是可以的，只是要加分号），定义解析完了才开始解析语句。如果位于块结尾，那么必须加上一个空语句`;`。
- 同break和continue一个道理，goto的跳转地址需要函数结束后再来填充，同样需要一个表来保存。新增枚举，第一个域是标号的哈希，第二是跳转地址在代码段中地址。实现类似于break。
```C++
enum Goto_list_domain { LabelHash = 0, JmpCodeAddress, GotoListSize };
```

## 自定义类型支持

### 做法

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
其中使用`UNION` ~ `STRUCT-1`的值来表示联合类型，用`STRUCT` ~ `PTR - 1`来表示结构体类型，即是支持400种自定义`union`，500种自定义`struct`，`enum`的话直接解释为`int`，所有都一样，就不区分了。数量不够用了改一下枚举值即可。

3. 还需要修改所有进行了类型检查的地方，以支持新增的类型或对自定义类型报错。
4. 为了支持union和struct，还需要特定的结构来存储成员的类型和名称等信息，这就需要新的结构了，独立于现在的符号表。它仍然是符号表的一部分。用于符号表项的`Value`域。具体实现后续再具体研究。
5. 需要考虑类型和变量冲突，作用域覆盖等细节，直接规定已经被定义的自定义类型不能声明同名变量和函数以规避这个问题。函数名感觉也应该这样做。同时已经被定义为全局变量也不能再被定义同名类型或者函数。

### 影响

添加了自定义类型之后的影响范围（针对union和struct）：
- 类型定义。
- 变量类型解析：全局、局部、函数参数。
- 自定义类型是否可以作为函数返回值类型，union和struct在ax中存不下。
- 能否使用运算符，除了enum当做int之外，union和struct有自己的取成员运算符，内置运算符基本都是不支持的。
    - 不应该作为类型转换的目标类型。
    - 支持`. ->`。
    - `sizeof()`得到大小。
    - 应该支持赋值运算符，可以直接使用`memcpy`做内存拷贝来实现。
- 复合类型：指针的操作和普通指针区别不大。

## enum

简单起见，解析到定义了命名enum类型的变量时，它的类型将成为int。相当于为定义了一个别名，就不为特定的枚举类型定义一个`Var_type`值了。

- Class域：EnumType。
- Type域：Enum，因为不区分不同枚举类型，所以这个域可能意义不大，暂且就这样设置，用Class域判断是枚举类型名即可。

文法修改：
```EBNF
type = int | char | user_defined_type;
user_defined_type = (enum | union | struct), id;
```

需要修改的地方：
- 类型定义。
- 全局变量、函数返回值类型定义。
- 局部变量、函数参数定义。
- 强制类型转换，sizeof运算符。

## union & struct

### 文法

### .和->运算符