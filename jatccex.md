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
    - [语法](#%E8%AF%AD%E6%B3%95)
    - [选择实现的部分](#%E9%80%89%E6%8B%A9%E5%AE%9E%E7%8E%B0%E7%9A%84%E9%83%A8%E5%88%86)
    - [类型解析实现](#%E7%B1%BB%E5%9E%8B%E8%A7%A3%E6%9E%90%E5%AE%9E%E7%8E%B0)
    - [类型支持](#%E7%B1%BB%E5%9E%8B%E6%94%AF%E6%8C%81)
    - [运算符支持](#%E8%BF%90%E7%AE%97%E7%AC%A6%E6%94%AF%E6%8C%81)

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
global_decl = enum_decl | func_decl | var_decl | struct_decl | union_decl;
enum_decl = enum, [id], "{", id, ["=", number], {",", id, ["=", number]} ,"}";
func_decl = ret_type, id, "(", param_decl, ")", "{", func_body, "}";
struct_decl = struct, [id], "{", var_decl, {var_decl}, "}", ";";
union_decl = union, [id], "{", var_decl, {var_decl}, "}", ";";
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

现在才发现原来C语言的`enum/union/struct`变量声明时必须要加`enum/union/struct`关键字。写了好几年C++（C++中不要求）的我震惊了，才理解为什么C语言里面定义结构都要用typedef定义一个同名的类型别名。当然这里不支持typedef，就遵循标准，其实还降低了语法分析的难度。
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
- 变量类型解析：全局、局部。
- 自定义类型是否可以作为函数返回值类型和函数参数，union和struct在ax中存不下。
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

要实现得先明白[union](https://zh.cppreference.com/w/c/language/union)和[struct](https://zh.cppreference.com/w/c/language/struct)语法是什么样的。最新版本加的新特性不用关新，只需要关心最基本功能。

### 语法

unoin和struct的文法：
```EBNF
global_decl = enum_decl | func_decl | var_decl | struct_decl | union_decl;
struct_decl = struct, [id], "{", {var_decl}, "}", ";";
union_decl = union, [id], "{", {var_decl}, "}", ";";
```
两者很类似，但有区别，语法细节；
- 名字是可选的，声明列表中允许任意数量变量声明，位域声明和静态断言声明。
- 不能为空。
- 不允许不完整类型的成员和函数类型的成员，但结构中可以允许末尾拥有一个不完整的柔性数组成员，像这样`int a[];`。
- 在结构体对象内，其成员的地址按照成员定义（及位域分配单元的地址）的顺序递增，能转型指向结构体的指针为指向其首成员（或者若首成员为位域，则指向其分配单元）的指针。类似的，能转型指向结构体首成员的指针为指向整个结构体的指针。在任意二个成员间和最后的成员后可能存在无名的填充字节，但首成员前不会有。结构体的大小至少与其成员的大小之和一样大。
- 联合体和结构体中，都支持类型为不带名字的结构体的无名结构体成员被称作**匿名结构体**。每个匿名结构体的成员被认为是外围结构体或联合体的成员。若外围结构体或联合体亦为匿名，则递归应用此规则。
```C++
struct v {
    union { // 匿名联合体
        struct { int i, j; }; // 匿名结构体
        struct { long k, l; } w;
    };
    int m;
} v1;

v1.i = 2;   // 合法
v1.k = 3;   // 非法：内层结构体非匿名
v1.w.k = 5; // 合法
```
- 都允许前置声明,隐藏任何先前在标签命名空间中声明的 `name` 的含义，并在当前作用域中声明 `name` 为新的结构体名，可以在之后定义该结构体。在定义出现之前，此结构体名拥有不完整类型。不完整类型不能直接在结构体或者联合体中引用，但可以通过指针引用。
```C++
struct name;
union name;
```
- 可以在局部作用域声明和定义结构和联合体。
- [结构体与联合体初始化](https://zh.cppreference.com/w/c/language/struct_initialization)：支持使用非空、花括号包围、逗号分隔的列表作为初始化器。
    - 初始化联合体时，初始化器列表必须只有一个成员，它初始化联合体的首个成员，除非使用指派初始化器。
    - 若结构体或联合体的成员是数组、结构体或联合体，则初始化器的花括号可以嵌套。某些情况还可以进行简化，支持指派符初始化。
    - 不能提供多余成员的初始化器。
- 结构体和联合体在其定义结束前不完整，结构体不能拥有其自身类型的成员。允许指向其自身类型的指针成员是允许的，而这通常用于实现链表或树的节点。
- 因为结构体声明不建立作用域，故在 结构体声明列表 中引入的嵌套类型、枚举及枚举项会在定义结构体的外围作用域可见。
```C++
struct node
{
    union head
    {
        int i;
        char c;
        void *p;
    } data;
    struct node *left;
    struct node *right;
};

union head h;
```

### 选择实现的部分

实现选择：
- 不支持位域声明，静态断言声明，不支持struct的柔性数组成员，因为连数组都没有支持。
- 不支持嵌套定义，结构体或者联合体中要使用结构体或者联合体成员可以将其放在外面定义。
- 不支持匿名结构体定义，因为需要名称来做哈希，没有类型名不好做哈希。
- 不支持定义结构时声明变量，enum也是类似的。感觉并不难做，只是没有必要，后续也可以考虑加上。
- 不支持在局部作用域声明和定义结构和联合体，仅能在全局作用域定义和声明。
- 支持前向声明。不支持的话，结构存在的意义可能会被大大削减，毕竟相互通过指针引用非常常见。
- 部分支持初始化：支持初始化器，支持最简单的嵌套初始化，不支持指派符。对于union也就仅支持仅有一个成员的初始化器。
- 结构体成员按照`sizeof(int)`大小对齐，大小是`sizeof(int)`的整数倍。
- 联合体取最大成员长度（一定是`sizeof(int)`的整数倍），最小是一倍的`sizeof(int)`，全部成员地址都一样，全部从联合体首地址开始存。
- `char`、 `int`、指针都占用`sizeof(int)`大小的字节，char只使用第一个字节。32就是4个字节，64位就是8个，最后大小和每个域的偏移地址都一定是`sizeof(int)`整数倍。
- 不支持结构和联合体的赋值，使用`memecpy`内存拷贝完成。
- 不支持结构体或者联合体作为函数参数类型和返回值类型，返回值是存在ax中的，存不下，存在栈中的话就和现在的架构相违背了，需要很多特殊处理。但依然支持指针传递参数和返回值。

支持的部分：
```EBNF
global_decl = enum_decl | func_decl | var_decl | struct_decl | union_decl;
struct_decl = struct, [id], "{", var_decl, {var_decl}, "}", ";";
union_decl = union, [id], "{", var_decl, {var_decl}, "}", ";";
```

### 类型解析实现

考虑最简单的实现方式，用一个链表节点来表示一个结构体或者联合体类型或者一个它们的域，一个域的信息有：变量名称哈希、变量类型、大小、在整个结构中的偏移（union中所有域都是0）。
```C++
// union or struct domain
struct us_domain
{
    int hash;
    int type;
    int size;
    int offset;
    struct us_domain* next;
};

struct us_domain* struct_symbols_list;
struct us_domain* union_symbols_list;
```
线性列表`struct_symbols_list`保存所有的结构和联合体。这些元素表示一个个类型，每个元素都是一个链表的首节点。节点中`next`域指向后续节点是它的成员按声明顺序存放。

举例：
```C++
struct node
{
    void *data;
    struct node *left;
    struct node *right;
};
union head
{
    int number;
    struct node n;
}
```
最终结构体的符号信息表中会是这样的；
```
struct_symbols_list:
first element:
[hash of node]     [hash of data]       [hash of left]           [hash of right] 
[type of node]     [type of void*]      [type of node*]          [type of node*]
[size of node]     [4]                  [4]                      [4]            
[0]                [0]                  [4]                      [8]           
[next]          -> [next]          ->   [next]            ->     [next]        -> 0

union_symbols_list
first element:
[hash of head]      [hash of number]   [hash of n]   
[type of head]      [type of int]      [type of node]
[size of head]      [size of int]      [size of node]
[0]                 [0]                [0]           
[next]          ->  [next]          -> [next]        
```
结构和联合的类型由`Var_type`表示，`union`从100开始，`struct`从500开始，如果是复合了一层指针就加一个指针`PTR`，每新定义一个类型就+1，各自减去`UNION/STRUCT`就是他们在`struct_symbols_list/union_symbols_list`中的下标。这个类型就保存在符号表的`Class`域中。前向声明时就可以填充了，然后在结构体成员信息表中添加这一项，但`next`域为空。
```C++
enum Var_type { CHAR = 0, INT, ENUM, UNION = 100, STRUCT = 500, PTR = 1000 };
```

### 类型支持

- 类型定义和前向声明，上一步已经做了。
- 全局变量声明、局部变量声明。
    - 全局变量，分配在data区，依赖于struct或者union的大小。从其符号信息表中获取。
    - 局部变量分配在栈上，栈从高地址向低地址生长，所以结构和联合体变量的地址应该是最后一个单元紧邻下一个变量内存的首地址。所占单元数量是`sizeof(struct xxx) / sizeof(int)`，附加到`ENT`操作数上。
    ```
    int a;
    struct node s;
    int b;

    分配内存:
    ...             High address        reletive address to bp
    variable a                          -1
    end of struct s                     -2
    ... 
    begin of struct s                   -(1+sizeof(struct node)/sizeof(int)) <---- address of s
    variable b
    ...             Low addresss
    ```
- 作为函数返回值类型支持、函数参数声明。
    - 因为函数返回值存储在ax中，是一个右值。如果用栈做保存会需要做很多多余的事情，选择不支持。
    - 参数声明可以做，但是函数调用时需要做结构的复制，考虑还是不做了，用指针就行。实现赋值后可以考虑看要不要支持。
    - 总是可以使用指针，指针和其他类型指针并无区别。

### 运算符支持

直接相关：
- 强制类型转换：
    - 指针复合类型转换直接做就行，用户应该为代码行为负责。
    - `struct/union`类型不支持直接作为转换目标类型，报错。
- `sizeof`运算符
    - 编译期查找结构体和联合体符号信息表中获取大小生成加载到`ax`的代码即可。
- 考虑左值和右值的处理。不支持右值，右值找不到地方存，仅支持左值。
    - 所以不会支持赋值运算符。
    - 不支持任何形式的结构体/联合体初始化器。
- 左值的处理：
    - 表达式中遇到标识符时，如果是变量，需要加载地址到ax，原先的处理已经完备。
- 新增对结构体和联合体左值的取成员运算符`. ->`支持。
    - `.` `->`优先级最高。
    - 对原始类型仅支持 `.`。
    - 一层指针支持`->`。
    - 统一支持，如果是`->`，这时候指针地址已经在ax中，是否有效应该由程序员确保。如果是`.`，则必须确保左边是一个左值，也就是上一条指令是`LC/LI`，取消加载以得到地址。然后生成代码`PUSH` `IMM offset` `ADD` `LI/LC`，对地址做偏移之后在ax中得到的偏移后的地址就是成员的地址，也是一个左值。查找成员类型并更改表达式类型，前面在做一些类型判断即可完美支持。联合体可以不做偏移指针的操作，约定都从联合体开始地址开始保存，只需要`LI/LC`。还可以简单做一个优化，如果是偏移`0`也就是第一个成员也不做。
- 指针类型：`* & + - ++ -- []`
    - 解引用和取地址就是对类型操作减去或者加上一个PTR，不需要修改。
    - 指针类型和整数相加，需要将整数乘以单元大小。对一层结构和联合体进行处理。
    - 指针与指针相减，结果需要除以单元大小。对一层结构和联合体进行处理。
    - 指针和整数相减，指针偏移，同样乘以单元大小。
    - `++ --`同理。
    - `[]`就是偏移，也同理。

总结：

- 对原始类型仅支持 `.` `sizeof` 操作符。
- 对一层结构体/联合体指针额外支持`->`操作符。
- 对所有层数的指针支持指针支持的通用操作 `+ - ++ -- [] * &`，但一层的时候需要考虑单元大小。
- 其他所有情况都不合法，进行类型检查和报错。主要是`= ?: || && | ^ & == != < > <= >= << >> * / % ()`提供报错信息，运算前统一检查，不在每个运算符中单独检查。
- 不对结构和联合支持赋值运算符，使用`memcpy`内存拷贝来做复制。

